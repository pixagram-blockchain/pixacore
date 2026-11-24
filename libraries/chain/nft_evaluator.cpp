#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/hive_evaluator.hpp>
#include <hive/chain/pixa_restricted_accounts.hpp>
#include <hive/chain/database.hpp>
#include <hive/chain/database_virtual_operations.hpp>
#include <hive/chain/detail/state/nft_object_multiindex.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <hive/chain/detail/state/comment_object_multiindex.hpp>
#include <hive/chain/evaluator_registry.hpp>

#include <fc/uint128.hpp>

#include <hive/protocol/hive_specialised_exceptions.hpp>

/**
  * Native NFTs (HF32): the rules of nft_operation.
  *
  * One evaluator serves the operation; each action of nft_action has its rule in nft_action_visitor.
  * The limits the rules compare against come from the database (database_nft.cpp), so a later
  * hardfork changes them there. No funds are ever held by the chain for an NFT: minting burns the
  * fee at once and a sale pays the seller and the creator in the same operation that changes the
  * owner, so there is nothing to refund, expire or account for in validate_invariants().
  *
  * A mint creates the NFT only. A copy gets its nft_edition_object when it is first offered (list)
  * or first leaves its creator (transfer, or a sale from the reserve). Until then the copy belongs to
  * the creator, was acquired at mint time and has no offer - every rule below reads a missing object
  * that way, so a mint costs the same whatever the number of copies.
  *
  * The copies without an object are the reserve. Copy 0 names it: the creator lists copy 0 to offer
  * every copy of the reserve at one price, unlists it to withdraw that offer, and a buyer of copy 0
  * gets the lowest-numbered copy still in the reserve. When the creator withdraws the offer of a copy
  * that never left him, its object is removed and the copy is back in the reserve.
  */

namespace hive { namespace chain {

HIVE_DEFINE_EVALUATOR( nft )

void register_nft_evaluators( evaluator_registry<operation>& registry )
{
  registry.register_evaluator< nft_evaluator >();
}

namespace
{

struct nft_action_visitor
{
  typedef void result_type;

  nft_action_visitor( database& db, const account_object& actor )
    : _db( db ), _actor( actor ), _now( db.head_block_time() ) {}

  /**
    * mint: the actor turns one of his root posts into an NFT with 1 to 10,000 copies and pays
    * 1.000 PXS per copy, burned. He owns every copy until he sells or gives it.
    */
  void operator()( const nft_mint& a ) const
  {
    // The post has to exist now. It is looked up only here: afterwards the NFT is found by its own
    // (creator, permlink) index and no action needs the post, which may be in the comment archive.
    comment post = _db.find_comment( _actor.get_id(), a.permlink );
    HIVE_CHAIN_STATE_ASSERT( post && "nft_mint_post_exists", _actor.get_name(),
      "Post ${a}/${p} does not exist.", ( "a", _actor.get_name() )( "p", a.permlink ) );
    HIVE_CHAIN_STATE_ASSERT( post->is_root() && "nft_mint_root_post", _actor.get_name(),
      "Only a root post can be minted, ${a}/${p} is a reply.", ( "a", _actor.get_name() )( "p", a.permlink ) );
    HIVE_CHAIN_STATE_ASSERT( _db.find_nft( _actor.get_id(), a.permlink ) == nullptr && "nft_mint_once", _actor.get_name(),
      "Post ${a}/${p} is already minted.", ( "a", _actor.get_name() )( "p", a.permlink ) );

    HIVE_CHAIN_LIMIT_ASSERT( a.editions <= _db.get_nft_max_editions() && "nft_mint_editions", a.editions,
      "An NFT can have at most ${max} edition(s), ${subject} requested.", ( "max", _db.get_nft_max_editions() ) );
    HIVE_CHAIN_LIMIT_ASSERT( a.royalty <= _db.get_nft_max_royalty() && "nft_mint_royalty", a.royalty,
      "NFT royalty can be at most ${max} basis points, ${subject} requested.", ( "max", _db.get_nft_max_royalty() ) );

    // The operation states the fee and it has to be exactly the fee for that many copies, so nobody
    // pays an amount other than the one that was signed.
    const HBD_asset required_fee = _db.get_nft_mint_fee( a.editions );
    const HBD_asset paid_fee( a.fee );
    HIVE_CHAIN_FEE_ASSERT( paid_fee == required_fee && "nft_mint_fee", required_fee,
      "Must pay the exact NFT mint fee. paid: ${p} fee: ${f}", ( "p", paid_fee )( "f", required_fee ) );

    temp_HBD_balance fee;
    _db.adjust_balance( _actor, fee, -paid_fee ); // fails when the minter does not have the PXS

    // the copies get no object here: each is created when first offered or given
    _db.create< nft_object >( _actor, a.permlink, _now, a.editions, a.royalty, a.license );

    // burned with everything else on the null account at the end of the block
    _db.adjust_balance( _db.get_account( HIVE_NULL_ACCOUNT ), fee, fee.as_asset() );
  }

  /**
    * list: the owner offers a copy at a fixed price in PXS, or replaces the offer he made before.
    * Copy 0 is the reserve: the creator offers every copy still in it at that one price.
    */
  void operator()( const nft_list& a ) const
  {
    const nft_object& nft = get_nft( a.author, a.permlink );
    const nft_edition_object* copy = nullptr;
    if( a.edition == 0 )
    {
      require_creator( nft, a.author, a.permlink );
    }
    else
    {
      copy = find_copy( nft, a.author, a.permlink, a.edition );
      require_owner( nft, copy, a.author, a.permlink, a.edition );
    }

    HIVE_CHAIN_ASSET_ASSERT( _db.is_nft_price_symbol( a.price.symbol ) && "nft_list_price_symbol", a.price,
      "An NFT can only be priced in PXS. Actual: ${subject}" );
    HIVE_CHAIN_TIME_ASSERT( a.expiration > _now && "nft_list_expiration", _actor.get_name(),
      "The offer has to expire in the future.", ( "now", _now )( "expiration", a.expiration ) );

    if( a.edition == 0 )
    {
      HIVE_CHAIN_STATE_ASSERT( nft.get_reserve() > 0 && "nft_list_reserve", _actor.get_name(),
        "No copy of ${a}/${p} is left in the reserve.", ( "a", a.author )( "p", a.permlink ) );
      _db.modify( nft, [&]( nft_object& n )
      {
        n.set_reserve_offer( a.price, a.expiration );
      } );
      return;
    }

    if( copy == nullptr )
    {
      // the creator offers a copy of the reserve on its own: it gets its object, still his since
      // the mint, and leaves the reserve
      copy = &_db.create< nft_edition_object >( nft, a.edition, _actor, nft.get_mint_time() );
      _db.modify( nft, [&]( nft_object& n )
      {
        n.remove_from_reserve( a.edition );
      } );
    }

    _db.modify( *copy, [&]( nft_edition_object& e )
    {
      e.set_offer( a.price, a.expiration );
    } );
  }

  /**
    * unlist: the owner withdraws his offer. An expired offer can still be removed this way.
    * Copy 0 withdraws the creator's offer of the reserve. A copy that never left its creator goes back
    * to the reserve: its object is removed.
    */
  void operator()( const nft_unlist& a ) const
  {
    const nft_object& nft = get_nft( a.author, a.permlink );
    if( a.edition == 0 )
    {
      require_creator( nft, a.author, a.permlink );
      HIVE_CHAIN_STATE_ASSERT( nft.has_reserve_offer() && "nft_unlist_reserve_offer", _actor.get_name(),
        "The reserve of ${a}/${p} is not offered for sale.", ( "a", a.author )( "p", a.permlink ) );
      _db.modify( nft, [&]( nft_object& n )
      {
        n.clear_reserve_offer();
      } );
      return;
    }

    const nft_edition_object* copy = find_copy( nft, a.author, a.permlink, a.edition );
    require_owner( nft, copy, a.author, a.permlink, a.edition );

    HIVE_CHAIN_STATE_ASSERT( copy != nullptr && copy->has_offer() && "nft_unlist_offer", _actor.get_name(),
      "Edition ${e} of ${a}/${p} is not offered for sale.", ( "e", a.edition )( "a", a.author )( "p", a.permlink ) );

    if( !copy->has_left_creator() )
    {
      // only its creator ever held this copy, and now it has no offer: it is a reserve copy again
      _db.modify( nft, [&]( nft_object& n )
      {
        n.return_to_reserve( a.edition );
      } );
      _db.remove( *copy );
      return;
    }

    _db.modify( *copy, [&]( nft_edition_object& e )
    {
      e.clear_offer();
    } );
  }

  /**
    * buy: the actor takes the offer. He pays the price in PXS, the creator receives the royalty
    * unless he is the seller, the seller receives the rest, and the actor becomes the owner - all
    * or nothing. Copy 0 buys from the reserve (buy_from_reserve).
    */
  void operator()( const nft_buy& a ) const
  {
    const nft_object& nft = get_nft( a.author, a.permlink );
    if( a.edition == 0 )
    {
      buy_from_reserve( nft, a );
      return;
    }
    const nft_edition_object* copy = find_copy( nft, a.author, a.permlink, a.edition );

    HIVE_CHAIN_STATE_ASSERT( copy != nullptr && copy->is_listed( _now ) && "nft_buy_listed", _actor.get_name(),
      "Edition ${e} of ${a}/${p} is not offered for sale.", ( "e", a.edition )( "a", a.author )( "p", a.permlink ) );
    HIVE_CHAIN_ASSET_ASSERT( a.price == copy->get_price() && "nft_buy_price", a.price,
      "The asking price is ${ask}, the operation offers ${subject}.", ( "ask", copy->get_price() ) );
    HIVE_CHAIN_PERMISSION_ASSERT( copy->get_owner() != _actor.get_id() && "nft_buy_own", _actor.get_name(),
      "${subject} already owns edition ${e} of ${a}/${p}.", ( "e", a.edition )( "a", a.author )( "p", a.permlink ) );

    const account_object& seller = _db.get_account( copy->get_owner() );
    const account_object& creator = _db.get_account( nft.get_creator() );
    const asset price = a.price;

    // The creator is paid on every sale by someone else. Rounded down to the precision of PXS;
    // 128 bits because price times basis points does not fit 64 for large prices.
    asset royalty( 0, price.symbol );
    if( seller.get_id() != creator.get_id() )
      royalty.amount = fc::uint128_to_int64( ( uint128_t( price.amount.value ) * nft.get_royalty() ) / HIVE_100_PERCENT );

    temp_balance payment( price.symbol );
    _db.adjust_balance( _actor, payment, -price ); // fails when the buyer cannot cover the price
    if( royalty.amount > 0 )
      _db.adjust_balance( creator, payment, royalty );
    _db.adjust_balance( seller, payment, price - royalty );

    const account_name_type seller_name = seller.get_name();
    _db.modify( *copy, [&]( nft_edition_object& e )
    {
      e.set_owner( _actor, _now ); // also ends the offer that was just taken
    } );

    push_virtual_operation( _db,
      nft_sold_operation( seller_name, _actor.get_name(), a.author, a.permlink, a.edition, price, royalty ) );
    if( royalty.amount > 0 )
      push_virtual_operation( _db,
        nft_royalty_operation( creator.get_name(), seller_name, _actor.get_name(), a.permlink, a.edition, royalty ) );
  }

  /**
    * transfer: the owner gives the copy away. No payment and therefore no royalty.
    * Giving it to the null account burns it: null signs nothing, so the copy stays there for good,
    * still numbered and still counted in the editions of the NFT, with no offer.
    */
  void operator()( const nft_transfer& a ) const
  {
    const nft_object& nft = get_nft( a.author, a.permlink );
    const nft_edition_object* copy = find_copy( nft, a.author, a.permlink, a.edition );
    require_owner( nft, copy, a.author, a.permlink, a.edition );
    const account_object& to = _db.get_account( a.to ); // prove it exists

    // The treasury has no use for a copy; burning is the transfer to null.
    HIVE_CHAIN_TREASURY_ASSERT( !_db.is_treasury( a.to ) && "nft_transfer_to_treasury", a.to,
      "An NFT cannot be transferred to the treasury ${subject}." );
    assert_not_pixa_ico( a.to, "(cannot receive an NFT)" );

    if( copy == nullptr )
    {
      // the copy leaves its creator for the first time: it gets its object, owned by the receiver,
      // and leaves the reserve
      _db.create< nft_edition_object >( nft, a.edition, to, _now );
      _db.modify( nft, [&]( nft_object& n )
      {
        n.remove_from_reserve( a.edition );
      } );
      return;
    }
    _db.modify( *copy, [&]( nft_edition_object& e )
    {
      e.set_owner( to, _now ); // also ends the offer of the previous owner
    } );
  }

private:
  /**
    * buy of copy 0: the actor takes the creator's offer of the reserve and gets the lowest-numbered
    * copy still in it. The creator is the seller, so he receives the whole price and there is no
    * royalty. nft_sold_operation names the copy that was sold.
    */
  void buy_from_reserve( const nft_object& nft, const nft_buy& a ) const
  {
    HIVE_CHAIN_STATE_ASSERT( nft.is_reserve_listed( _now ) && "nft_buy_reserve_listed", _actor.get_name(),
      "The reserve of ${a}/${p} is not offered for sale.", ( "a", a.author )( "p", a.permlink ) );
    HIVE_CHAIN_ASSET_ASSERT( a.price == nft.get_reserve_price() && "nft_buy_reserve_price", a.price,
      "The asking price is ${ask}, the operation offers ${subject}.", ( "ask", nft.get_reserve_price() ) );
    HIVE_CHAIN_PERMISSION_ASSERT( nft.get_creator() != _actor.get_id() && "nft_buy_reserve_own", _actor.get_name(),
      "${subject} created ${a}/${p} and already holds its reserve.", ( "a", a.author )( "p", a.permlink ) );

    const uint16_t edition = _db.find_nft_reserve_copy( nft );
    HIVE_CHAIN_UNREACHABLE_CODE_ASSERT( edition >= 1 && edition <= nft.get_editions() && "nft_reserve_copy",
      "A reserve that is not empty has a copy without an object." );

    const account_object& creator = _db.get_account( nft.get_creator() );
    const asset price = a.price;

    temp_balance payment( price.symbol );
    _db.adjust_balance( _actor, payment, -price ); // fails when the buyer cannot cover the price
    _db.adjust_balance( creator, payment, price );

    _db.create< nft_edition_object >( nft, edition, _actor, _now );
    _db.modify( nft, [&]( nft_object& n )
    {
      n.remove_from_reserve( edition ); // also ends the reserve offer when this was the last copy
      n.skip_reserve_to( edition + 1 ); // the search crossed only copies with an object
    } );

    push_virtual_operation( _db,
      nft_sold_operation( creator.get_name(), _actor.get_name(), a.author, a.permlink, edition, price, asset( 0, price.symbol ) ) );
  }

  const nft_object& get_nft( const account_name_type& author, const string& permlink ) const
  {
    const nft_object* nft = _db.find_nft( author, permlink );
    HIVE_CHAIN_STATE_ASSERT( nft != nullptr && "nft_exists", author,
      "Post ${a}/${p} is not minted.", ( "a", author )( "p", permlink ) );
    return *nft;
  }

  // The object of copy `edition`, or null when the copy is still, untouched, in the reserve.
  // Fails when the NFT has fewer copies; copy 0, the reserve, is handled before this is called.
  const nft_edition_object* find_copy( const nft_object& nft, const account_name_type& author, const string& permlink,
    uint16_t edition ) const
  {
    HIVE_CHAIN_STATE_ASSERT( edition >= 1 && edition <= nft.get_editions() && "nft_edition_exists", author,
      "${a}/${p} has ${n} edition(s), edition ${e} does not exist.",
      ( "a", author )( "p", permlink )( "n", nft.get_editions() )( "e", edition ) );
    return _db.find_nft_edition( nft, edition );
  }

  // the owner of a copy: its object says, and a copy without one is the creator's
  static account_id_type owner_of( const nft_object& nft, const nft_edition_object* copy )
  {
    return copy != nullptr ? copy->get_owner() : nft.get_creator();
  }

  void require_owner( const nft_object& nft, const nft_edition_object* copy, const account_name_type& author,
    const string& permlink, uint16_t edition ) const
  {
    HIVE_CHAIN_PERMISSION_ASSERT( owner_of( nft, copy ) == _actor.get_id() && "nft_owner", _actor.get_name(),
      "${subject} does not own edition ${e} of ${a}/${p}.", ( "e", edition )( "a", author )( "p", permlink ) );
  }

  // the reserve is the creator's: only he offers it or withdraws the offer
  void require_creator( const nft_object& nft, const account_name_type& author, const string& permlink ) const
  {
    HIVE_CHAIN_PERMISSION_ASSERT( nft.get_creator() == _actor.get_id() && "nft_reserve_creator", _actor.get_name(),
      "Only the creator of ${a}/${p} can offer its reserve, ${subject} cannot.", ( "a", author )( "p", permlink ) );
  }

  database&             _db;
  const account_object& _actor;
  const time_point_sec  _now;
};

} // anonymous

void nft_evaluator::do_apply( const nft_operation& o )
{ try {
  HIVE_CHAIN_HARDFORK_ASSERT( _db.has_hardfork( HIVE_HARDFORK_1_32_NFT ) && "nft_operation",
    "NFT operations are not enabled until hardfork ${hf}.", ( "hf", HIVE_HARDFORK_1_32_NFT ) );
  assert_not_pixa_ico( o.account, "(NFT operations not allowed)" );

  const account_object& actor = _db.get_account( o.account );
  o.action.visit( nft_action_visitor( _db, actor ) );
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // hive::chain

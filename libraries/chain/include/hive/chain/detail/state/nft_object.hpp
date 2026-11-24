#pragma once
#include <hive/chain/hive_fwd.hpp>
#include <hive/protocol/asset.hpp>
#include <hive/protocol/config.hpp>
#include <hive/chain/hive_object_types.hpp>
#include <hive/chain/detail/state/account_object.hpp>

#include <fc/crypto/sha256.hpp>

namespace hive { namespace chain {

using hive::protocol::asset;

/**
  * A minted post (HF32).
  *
  * Consensus keeps no post content, and the object that holds a post's permlink is removed when
  * the post is paid out, so the NFT stores its own identity: the creator and the permlink. Its terms
  * (copies, royalty, license) are set at mint and there is no way to change one afterwards - that is
  * what makes the royalty and the license immutable. Only the reserve fields below change.
  *
  * The license text itself is not kept in state: it can be up to 72 KiB and stays in the mint
  * operation, in the block log. The NFT keeps the SHA-256 of that text and its length, so anyone who
  * reads the text elsewhere can check it is the one the chain recorded.
  *
  * The copies of an NFT do not get an object at mint. A copy that was never offered and never left
  * its creator has no nft_edition_object: it is the creator's, acquired at mint time, with no offer.
  * Its object is created the first time it is offered or given (see nft_evaluator.cpp).
  *
  * The copies without an object are the reserve: they never left the creator and have no offer of
  * their own. The creator can offer the whole reserve at one price (the offer of "copy 0"); each buyer
  * then gets the lowest-numbered copy still in the reserve. The NFT counts the reserve and remembers
  * where to start looking for that copy, so a sale from the reserve does not walk every copy.
  */
class nft_object : public object< nft_object_type, nft_object, std::true_type >
{
  CHAINBASE_OBJECT( nft_object );
public:
  template< typename Allocator >
  nft_object( allocator< Allocator > a, uint64_t _id,
    const account_object& _creator, const std::string& _permlink, const time_point_sec& _mint_time,
    uint16_t _editions, uint16_t _royalty, const std::string& _license )
  : id( _id ), creator( _creator.get_id() ), permlink( a ), mint_time( _mint_time ),
    editions( _editions ), royalty( _royalty ), license_size( static_cast< uint32_t >( _license.size() ) ),
    license_sha256( fc::sha256::hash( _license ) ), reserve( _editions )
  {
    from_string( permlink, _permlink );
  }

  //author of the minted post
  account_id_type get_creator() const { return creator; }
  //permlink of the minted post
  const shared_string& get_permlink() const { return permlink; }
  //head block time of the block that minted the post
  time_point_sec get_mint_time() const { return mint_time; }
  //number of editions (copies); the editions are numbered from 1
  uint16_t get_editions() const { return editions; }
  //share of each sale price paid to the creator, in basis points
  uint16_t get_royalty() const { return royalty; }
  //length in bytes of the license text recorded by the mint operation
  uint32_t get_license_size() const { return license_size; }
  //SHA-256 of the license text recorded by the mint operation
  const fc::sha256& get_license_sha256() const { return license_sha256; }

  //number of copies in the reserve: still with the creator since the mint, without an offer of their own
  uint16_t get_reserve() const { return reserve; }
  //no copy numbered below this one is in the reserve; the search for the next reserve copy starts here
  uint16_t get_reserve_start() const { return reserve_start; }
  //the creator's price for any copy of the reserve; an amount of zero means that the reserve is not offered
  const asset& get_reserve_price() const { return reserve_price; }
  //the reserve offer cannot be taken at or after this time
  time_point_sec get_reserve_expiration() const { return reserve_expiration; }
  //tells if a copy can be bought from the reserve at given time
  bool is_reserve_listed( const time_point_sec& now ) const
  {
    return reserve > 0 && reserve_price.amount > 0 && now < reserve_expiration;
  }
  //tells if a reserve offer was recorded (it may have expired)
  bool has_reserve_offer() const { return reserve_price.amount > 0; }

  void set_reserve_offer( const asset& _price, const time_point_sec& _expiration )
  {
    reserve_price = _price;
    reserve_expiration = _expiration;
  }
  void clear_reserve_offer()
  {
    reserve_price = asset( 0, PXS_SYMBOL );
    reserve_expiration = time_point_sec();
  }
  //copy `edition` got its object, so it left the reserve; the offer ends with the last copy
  void remove_from_reserve( uint16_t edition )
  {
    --reserve;
    if( edition == reserve_start )
      ++reserve_start;
    if( reserve == 0 )
      clear_reserve_offer();
  }
  //every copy below `start` is known to have an object (after a search of the reserve)
  void skip_reserve_to( uint16_t start ) { reserve_start = start; }
  //copy `edition` lost its object: it never left the creator and has no offer any more
  void return_to_reserve( uint16_t edition )
  {
    ++reserve;
    if( edition < reserve_start )
      reserve_start = edition;
  }

  size_t get_dynamic_alloc() const
  {
    return permlink.capacity() * sizeof( decltype( permlink )::value_type );
  }

private:
  account_id_type creator;
  shared_string   permlink;
  time_point_sec  mint_time;
  uint16_t        editions = 1;
  uint16_t        royalty = 0;
  uint32_t        license_size = 0;
  fc::sha256      license_sha256;
  uint16_t        reserve = 1;
  uint16_t        reserve_start = 1;
  time_point_sec  reserve_expiration;
  asset           reserve_price = asset( 0, PXS_SYMBOL );

  CHAINBASE_UNPACK_CONSTRUCTOR( nft_object, (permlink) );
};

/**
  * One numbered copy of a minted post (HF32): who owns it and whether it is offered for sale.
  *
  * Created the first time the copy is offered or leaves its creator; before that the copy has no
  * object and is in the reserve of the creator (see nft_object). Removed only when the creator
  * withdraws the offer of a copy that never left him, which puts the copy back in the reserve.
  *
  * The object holds no funds. An offer is only an asking price, in PXS; the buyer pays the seller and
  * the creator directly in the operation that takes it. An offer whose expiration has passed is
  * treated as absent by every rule, so nothing has to clean it up in block processing.
  * A copy given to the null account is burned: null signs nothing, so the copy never moves again.
  */
class nft_edition_object : public object< nft_edition_object_type, nft_edition_object >
{
  CHAINBASE_OBJECT( nft_edition_object );
public:
  template< typename Allocator >
  nft_edition_object( allocator< Allocator > a, uint64_t _id,
    const nft_object& _nft, uint16_t _edition, const account_object& _owner, const time_point_sec& _acquired )
  : id( _id ), nft( _nft.get_id() ), edition( _edition ), left_creator( _owner.get_id() != _nft.get_creator() ),
    owner( _owner.get_id() ), acquired( _acquired )
  {}

  nft_id_type get_nft() const { return nft; }
  //number of this copy, from 1 to nft_object::get_editions()
  uint16_t get_edition() const { return edition; }
  account_id_type get_owner() const { return owner; }
  //time the current owner got the edition (mint time while the creator still holds it)
  time_point_sec get_acquired() const { return acquired; }
  //tells if the copy was ever given or sold; one that came back to its creator still did
  bool has_left_creator() const { return left_creator; }

  //asking price; an amount of zero means that the edition was not offered
  const asset& get_price() const { return price; }
  //the offer cannot be taken at or after this time
  time_point_sec get_expiration() const { return expiration; }
  //tells if the edition can be bought at given time
  bool is_listed( const time_point_sec& now ) const { return price.amount > 0 && now < expiration; }
  //tells if an offer was recorded (it may have expired)
  bool has_offer() const { return price.amount > 0; }

  void set_offer( const asset& _price, const time_point_sec& _expiration )
  {
    price = _price;
    expiration = _expiration;
  }
  void clear_offer()
  {
    price = no_offer();
    expiration = time_point_sec();
  }
  //changes the owner; an offer made by the previous owner ends with it
  void set_owner( const account_object& _owner, const time_point_sec& _now )
  {
    owner = _owner.get_id();
    acquired = _now;
    left_creator = true;
    clear_offer();
  }

private:
  //the price of a copy that is not offered: zero, in a fixed asset so that every node stores the same bytes
  static asset no_offer() { return asset( 0, PXS_SYMBOL ); }

  nft_id_type     nft;
  uint16_t        edition = 1;
  bool            left_creator = false;
  account_id_type owner;
  time_point_sec  acquired;
  asset           price = no_offer();
  time_point_sec  expiration;

  CHAINBASE_UNPACK_CONSTRUCTOR( nft_edition_object );
};

} } // hive::chain

FC_REFLECT( hive::chain::nft_object,
          (id)(creator)(permlink)(mint_time)(editions)(royalty)(license_size)(license_sha256)
          (reserve)(reserve_start)(reserve_expiration)(reserve_price) )

FC_REFLECT( hive::chain::nft_edition_object,
          (id)(nft)(edition)(left_creator)(owner)(acquired)(price)(expiration) )

#ifdef IS_TEST_NET

// Hardfork 32: native NFTs (nft_operation, libraries/chain/nft_evaluator.cpp) and the new voting bar
// (database_hardfork.cpp, util/manabar.hpp, hf20_vote_evaluator).

#include <boost/test/unit_test.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <hive/chain/detail/state/comment_object_multiindex.hpp>
#include <hive/chain/detail/state/nft_object_multiindex.hpp>
#include <hive/chain/detail/state/witness_objects_multiindex.hpp>
#include <hive/chain/rc/resource_count.hpp>
#include <hive/chain/rc/resource_sizes.hpp>
#include <hive/plugins/account_history_rocksdb/account_history_rocksdb_objects_multiindex.hpp>
#include <hive/protocol/forward_impacted.hpp>

#include <fc/crypto/sha256.hpp>
#include <fc/io/json.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

using namespace hive::chain;
using namespace hive::protocol;

// The numbers every indexer downstream relies on. nft_operation took the slot of a placeholder,
// so no other operation moved; the two virtual operations were appended.
static_assert( operation::tag< nft_operation >::value == 37, "nft_operation has to keep the number 37" );
static_assert( operation::tag< decline_voting_rights_operation >::value == 36, "an operation number moved" );
static_assert( operation::tag< set_reset_account_operation >::value == 38, "an operation number moved" );
static_assert( operation::tag< recurrent_transfer_operation >::value == 49, "an operation number moved" );
static_assert( operation::tag< fill_convert_request_operation >::value == 50, "a virtual operation number moved" );
static_assert( operation::tag< declined_voting_rights_operation >::value == 92, "a virtual operation number moved" );
static_assert( operation::tag< nft_sold_operation >::value == 93, "nft_sold_operation has to keep the number 93" );
static_assert( operation::tag< nft_royalty_operation >::value == 94, "nft_royalty_operation has to keep the number 94" );
static_assert( nft_action::tag< nft_mint >::value == 0 && nft_action::tag< nft_list >::value == 1 &&
  nft_action::tag< nft_unlist >::value == 2 && nft_action::tag< nft_buy >::value == 3 &&
  nft_action::tag< nft_transfer >::value == 4, "the order of the NFT actions is their binary tag" );

namespace
{
struct hf32_database_fixture : hardfork_database_fixture
{
  hf32_database_fixture()
    : hardfork_database_fixture( database_fixture::shared_file_size_small, HIVE_HARDFORK_1_31 )
  {
    generate_block();
  }

  void activate()
  {
    // A direct set_hardfork in a pending session is undone by generate_block.
    // Replay this activation in the debug transaction committed by that block.
    db_plugin->debug_update( []( database& chain ) { chain.set_hardfork( HIVE_HARDFORK_1_32 ); } );
    generate_block();
    BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_32_NFT ) );
  }

  nft_operation mint( const std::string& account, const std::string& permlink, uint16_t royalty = 0,
    const std::string& license = "PIXA_LICENSE/1.0", uint16_t copies = 1 )
  {
    nft_mint action;
    action.permlink = permlink;
    action.editions = copies;
    action.royalty = royalty;
    action.license = license;
    action.fee = db->get_nft_mint_fee( copies ).to_asset();
    nft_operation op;
    op.account = account;
    op.action = action;
    return op;
  }

  nft_operation list( const std::string& account, const std::string& author, const std::string& permlink,
    const asset& price, fc::time_point_sec expiration = fc::time_point_sec::maximum(), uint16_t copy = 1 )
  {
    nft_list action;
    action.author = author;
    action.permlink = permlink;
    action.edition = copy;
    action.price = price;
    action.expiration = expiration;
    nft_operation op;
    op.account = account;
    op.action = action;
    return op;
  }

  nft_operation unlist( const std::string& account, const std::string& author, const std::string& permlink, uint16_t copy = 1 )
  {
    nft_unlist action;
    action.author = author;
    action.permlink = permlink;
    action.edition = copy;
    nft_operation op;
    op.account = account;
    op.action = action;
    return op;
  }

  nft_operation buy( const std::string& account, const std::string& author, const std::string& permlink, const asset& price,
    uint16_t copy = 1 )
  {
    nft_buy action;
    action.author = author;
    action.permlink = permlink;
    action.edition = copy;
    action.price = price;
    nft_operation op;
    op.account = account;
    op.action = action;
    return op;
  }

  nft_operation give( const std::string& account, const std::string& author, const std::string& permlink, const std::string& to,
    uint16_t copy = 1 )
  {
    nft_transfer action;
    action.author = author;
    action.permlink = permlink;
    action.edition = copy;
    action.to = to;
    nft_operation op;
    op.account = account;
    op.action = action;
    return op;
  }

  // the object of a copy, which it has once it was offered or moved
  const nft_edition_object& edition( const std::string& author, const std::string& permlink, uint16_t copy = 1 )
  {
    const nft_edition_object* found = find_edition( author, permlink, copy );
    BOOST_REQUIRE( found != nullptr );
    return *found;
  }

  const nft_edition_object* find_edition( const std::string& author, const std::string& permlink, uint16_t copy = 1 )
  {
    const nft_object* nft = db->find_nft( account_name_type( author ), permlink );
    BOOST_REQUIRE( nft != nullptr );
    return db->find_nft_edition( *nft, copy );
  }

  // how many copies, of all NFTs, have an object
  size_t copy_objects() const
  {
    return db->get_index< nft_edition_index, by_id >().size();
  }

  // the most recent operation of given type in the head block. The history plugin records the
  // operations of a transaction when its block is applied, not when the transaction is pushed, so
  // generate the block first.
  template< typename Op >
  fc::optional< Op > last_operation()
  {
    const auto& ops = db->get_index< hive::plugins::account_history_rocksdb::volatile_operation_index, by_id >();
    for( auto it = ops.rbegin(); it != ops.rend() && it->block == db->head_block_num(); ++it )
    {
      operation op = fc::raw::unpack_from_buffer< operation >( it->serialized_op );
      if( op.which() == operation::tag< Op >::value )
        return op.get< Op >();
    }
    return fc::optional< Op >();
  }
};

const asset pxs( int64_t satoshis ) { return asset( satoshis, PXS_SYMBOL ); }
const asset pixa( int64_t satoshis ) { return asset( satoshis, PIXA_SYMBOL ); }
const fc::time_point_sec never = fc::time_point_sec::maximum();
// a license as long as the format allows: 72 KiB of UTF-8, multi-byte characters included
const std::string longest_license()
{
  std::string text;
  while( text.size() + 3 <= PIXA_NFT_MAX_LICENSE_LENGTH )
    text += "\xC3\xA9x"; // "éx": a two-byte character and an ASCII one
  text.append( PIXA_NFT_MAX_LICENSE_LENGTH - text.size(), 'y' );
  return text;
}
}

BOOST_FIXTURE_TEST_SUITE( hf32_tests, hf32_database_fixture )

BOOST_AUTO_TEST_CASE( nothing_changes_before_the_hardfork )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice) );
  fund( "alice", PXS_asset( 100'000 ) );
  post_comment( "alice", "art", "title", "body", "pixelart", alice_post_key );
  generate_block();

  BOOST_REQUIRE( !db->has_hardfork( HIVE_HARDFORK_1_32_NFT ) );
  HIVE_REQUIRE_ASSERT( push_transaction( mint( "alice", "art" ), alice_private_key ),
    "_db.has_hardfork( HIVE_HARDFORK_1_32_NFT ) && \"nft_operation\"" );
  BOOST_REQUIRE( db->find_nft( account_name_type( "alice" ), "art" ) == nullptr );

  // a post can still be edited and deleted as before
  post_comment( "alice", "art", "title", "edited", "pixelart", alice_post_key );
  generate_block();
  delete_comment( "alice", "art", alice_post_key );
  generate_block();
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( mint_rules )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice)(bob) );
  fund( "alice", PXS_asset( 100'000 ) );
  post_comment( "alice", "art", "title", "body", "pixelart", alice_post_key );
  post_comment_to_comment( "bob", "reply", "title", "body", "alice", "art", bob_post_key );
  generate_block();
  activate();

  BOOST_TEST_MESSAGE( "--- the post has to exist, be a root post and belong to the minter" );
  HIVE_REQUIRE_ASSERT( push_transaction( mint( "alice", "no-such-post" ), alice_private_key ),
    "post && \"nft_mint_post_exists\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( mint( "bob", "reply" ), bob_private_key ),
    "post->is_root() && \"nft_mint_root_post\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( mint( "bob", "art" ), bob_private_key ),
    "post && \"nft_mint_post_exists\"" ); // bob has no post "art": alice's cannot be minted by him

  BOOST_TEST_MESSAGE( "--- the limits consensus applies" );
  {
    nft_operation op = mint( "alice", "art", 0, "PIXA_LICENSE/1.0", PIXA_NFT_MAX_EDITIONS + 1 );
    HIVE_REQUIRE_THROW( push_transaction( op, alice_private_key ), fc::assert_exception ); // 10,001 copies
    op = mint( "alice", "art", PIXA_NFT_MAX_ROYALTY + 1 );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ),
      "a.royalty <= _db.get_nft_max_royalty() && \"nft_mint_royalty\"" );
    op = mint( "alice", "art", 0, longest_license() + "z" );
    HIVE_REQUIRE_THROW( push_transaction( op, alice_private_key ), fc::assert_exception );
  }

  BOOST_TEST_MESSAGE( "--- minting burns the fee and creates the NFT; its copy gets no object yet" );
  const std::string license = "Terms of use\nThe holder may display this artwork.\n";
  const PXS_asset fee = db->get_nft_mint_fee( 1 );
  BOOST_REQUIRE( fee == PXS_asset( 1'000 ) ); // 1.000 PXS
  const PXS_asset alice_before = get_pxs_balance( "alice" );
  push_transaction( mint( "alice", "art", 1000, license ), alice_private_key );
  BOOST_REQUIRE( get_pxs_balance( "alice" ) == alice_before - fee );
  BOOST_REQUIRE( get_pxs_balance( HIVE_NULL_ACCOUNT ) == fee );

  const nft_object* nft = db->find_nft( account_name_type( "alice" ), "art" );
  BOOST_REQUIRE( nft != nullptr );
  BOOST_REQUIRE( nft->get_creator() == alice_id );
  BOOST_REQUIRE_EQUAL( to_string( nft->get_permlink() ), "art" );
  BOOST_REQUIRE_EQUAL( nft->get_editions(), 1 );
  BOOST_REQUIRE_EQUAL( nft->get_royalty(), 1000 );
  BOOST_REQUIRE_EQUAL( nft->get_license_size(), license.size() );
  BOOST_REQUIRE( nft->get_license_sha256() == fc::sha256::hash( license ) );
  BOOST_REQUIRE( find_edition( "alice", "art" ) == nullptr ); // alice's, unoffered, without an object
  BOOST_REQUIRE_EQUAL( copy_objects(), 0u );

  generate_block(); // the null account is cleared at the end of the block: the fee is burned
  BOOST_REQUIRE( get_pxs_balance( HIVE_NULL_ACCOUNT ) == PXS_asset( 0 ) );

  BOOST_TEST_MESSAGE( "--- a post is minted once" );
  HIVE_REQUIRE_ASSERT( push_transaction( mint( "alice", "art" ), alice_private_key ),
    "_db.find_nft( _actor.get_id(), a.permlink ) == nullptr && \"nft_mint_once\"" );

  BOOST_TEST_MESSAGE( "--- a minted post is frozen" );
  comment_operation edit;
  edit.author = "alice";
  edit.permlink = "art";
  edit.parent_permlink = "pixelart";
  edit.title = "title";
  edit.body = "another picture";
  HIVE_REQUIRE_ASSERT( push_transaction( edit, alice_post_key ),
    "_db.find_nft( auth.get_id(), o.permlink ) == nullptr && \"nft_post_edit\"" );
  delete_comment_operation del;
  del.author = "alice";
  del.permlink = "art";
  HIVE_REQUIRE_ASSERT( push_transaction( del, alice_post_key ),
    "_db.find_nft( o.author, o.permlink ) == nullptr && \"nft_post_delete\"" );
  // replies to it are ordinary comments and stay editable
  post_comment_to_comment( "bob", "reply", "title", "edited reply", "alice", "art", bob_post_key );
  generate_block();
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( mint_fee_per_copy )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice) );
  issue_funds( "alice", PXS_asset( 10'100'000 ) ); // 10,100 PXS
  post_comment( "alice", "first", "title", "body", "pixelart", alice_post_key );
  generate_blocks( db->head_block_time() + HIVE_MIN_ROOT_COMMENT_INTERVAL + fc::seconds( HIVE_BLOCK_INTERVAL ) );
  post_comment( "alice", "second", "title", "body", "pixelart", alice_post_key );
  generate_block();
  activate();

  BOOST_TEST_MESSAGE( "--- 1.000 PXS per copy, whatever the account creation fee" );
  BOOST_REQUIRE( db->get_nft_mint_fee( 5 ) == PXS_asset( 5'000 ) );
  BOOST_REQUIRE( db->get_nft_mint_fee( PIXA_NFT_MAX_EDITIONS ) == PXS_asset( 10'000'000 ) );
  set_account_creation_fee( PIXA_asset( 35'000 ) );
  BOOST_REQUIRE( db->get_nft_mint_fee( 5 ) == PXS_asset( 5'000 ) );

  BOOST_TEST_MESSAGE( "--- the fee has to be exactly the fee for that many copies" );
  {
    nft_operation op = mint( "alice", "first", 0, "PIXA_LICENSE/1.0", 5 );
    op.action.get< nft_mint >().fee = pxs( 1'000 ); // the fee of one copy
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "paid_fee == required_fee && \"nft_mint_fee\"" );
    op.action.get< nft_mint >().fee = pxs( 5'001 );
    HIVE_REQUIRE_ASSERT( push_transaction( op, alice_private_key ), "paid_fee == required_fee && \"nft_mint_fee\"" );
    op.action.get< nft_mint >().fee = pixa( 5'000 ); // the fee is in PXS (format check)
    HIVE_REQUIRE_THROW( push_transaction( op, alice_private_key ), fc::assert_exception );
  }

  BOOST_TEST_MESSAGE( "--- 10,000 copies: 10,000 PXS, burned, and still no copy object" );
  const PXS_asset before = get_pxs_balance( "alice" );
  push_transaction( mint( "alice", "first", 0, "PIXA_LICENSE/1.0", PIXA_NFT_MAX_EDITIONS ), alice_private_key );
  BOOST_REQUIRE( get_pxs_balance( "alice" ) == before - PXS_asset( 10'000'000 ) );
  BOOST_REQUIRE( get_pxs_balance( HIVE_NULL_ACCOUNT ) == PXS_asset( 10'000'000 ) );
  BOOST_REQUIRE_EQUAL( copy_objects(), 0u );
  generate_block();
  BOOST_REQUIRE( get_pxs_balance( HIVE_NULL_ACCOUNT ) == PXS_asset( 0 ) );

  BOOST_TEST_MESSAGE( "--- without the PXS for the fee nothing is minted" );
  BOOST_REQUIRE( get_pxs_balance( "alice" ) < db->get_nft_mint_fee( 101 ) );
  HIVE_REQUIRE_THROW( push_transaction( mint( "alice", "second", 0, "PIXA_LICENSE/1.0", 101 ), alice_private_key ),
    fc::assert_exception );
  BOOST_REQUIRE( db->find_nft( account_name_type( "alice" ), "second" ) == nullptr );
  validate_database(); // the PXS supply went down by the burned fee
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( list_buy_and_royalty )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice)(bob)(carol) );
  fund( "alice", PXS_asset( 100'000 ) );
  fund( "bob", PXS_asset( 100'000 ) );
  fund( "carol", PXS_asset( 100'000 ) );
  post_comment( "alice", "art", "title", "body", "pixelart", alice_post_key );
  generate_block();
  activate();
  push_transaction( mint( "alice", "art", 1000 ), alice_private_key ); // 10% royalty
  generate_block();

  BOOST_TEST_MESSAGE( "--- only the owner lists, in PXS, with an expiration in the future" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "bob", "alice", "art", pxs( 50'000 ) ), bob_private_key ),
    "owner_of( nft, copy ) == _actor.get_id() && \"nft_owner\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", "art", pxs( 50'000 ), db->head_block_time() ), alice_private_key ),
    "a.expiration > _now && \"nft_list_expiration\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", "art", pixa( 50'000 ) ), alice_private_key ),
    "_db.is_nft_price_symbol( a.price.symbol ) && \"nft_list_price_symbol\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", "nothing", pxs( 50'000 ) ), alice_private_key ),
    "nft != nullptr && \"nft_exists\"" );
  // a permlink that merely starts like the minted one, up to a NUL byte, names nothing
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", std::string( "art\0x", 5 ), pxs( 50'000 ) ), alice_private_key ),
    "nft != nullptr && \"nft_exists\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "bob", "alice", "art", pxs( 50'000 ) ), bob_private_key ),
    "copy != nullptr && copy->is_listed( _now ) && \"nft_buy_listed\"" );
  BOOST_REQUIRE( find_edition( "alice", "art" ) == nullptr ); // nothing above created the copy's object

  push_transaction( list( "alice", "alice", "art", pxs( 50'000 ) ), alice_private_key );
  BOOST_REQUIRE( edition( "alice", "art" ).get_price() == pxs( 50'000 ) ); // the first offer created it
  BOOST_REQUIRE( edition( "alice", "art" ).get_owner() == alice_id );
  BOOST_REQUIRE( edition( "alice", "art" ).get_acquired() == db->find_nft( account_name_type( "alice" ), "art" )->get_mint_time() );
  generate_block();

  BOOST_TEST_MESSAGE( "--- the buyer pays the asking price and nothing else" );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "bob", "alice", "art", pxs( 49'999 ) ), bob_private_key ),
    "a.price == copy->get_price() && \"nft_buy_price\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "bob", "alice", "art", pixa( 50'000 ) ), bob_private_key ),
    "a.price == copy->get_price() && \"nft_buy_price\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "alice", "alice", "art", pxs( 50'000 ) ), alice_private_key ),
    "copy->get_owner() != _actor.get_id() && \"nft_buy_own\"" );

  BOOST_TEST_MESSAGE( "--- first sale, by the creator: no royalty" );
  {
    const PXS_asset alice_before = get_pxs_balance( "alice" );
    const PXS_asset bob_before = get_pxs_balance( "bob" );
    push_transaction( buy( "bob", "alice", "art", pxs( 50'000 ) ), bob_private_key );
    BOOST_REQUIRE( get_pxs_balance( "bob" ) == bob_before - PXS_asset( 50'000 ) );
    BOOST_REQUIRE( get_pxs_balance( "alice" ) == alice_before + PXS_asset( 50'000 ) );
    BOOST_REQUIRE( edition( "alice", "art" ).get_owner() == bob_id );
    BOOST_REQUIRE( !edition( "alice", "art" ).has_offer() );

    generate_block();
    const auto sold = last_operation< nft_sold_operation >();
    BOOST_REQUIRE( sold.valid() );
    BOOST_REQUIRE_EQUAL( sold->seller, "alice" );
    BOOST_REQUIRE_EQUAL( sold->buyer, "bob" );
    BOOST_REQUIRE( sold->price == pxs( 50'000 ) && sold->royalty == pxs( 0 ) );
    BOOST_REQUIRE( !last_operation< nft_royalty_operation >().valid() );
  }

  BOOST_TEST_MESSAGE( "--- resale: the creator is paid the royalty fixed at mint" );
  {
    push_transaction( list( "bob", "alice", "art", pxs( 80'000 ) ), bob_private_key );
    push_transaction( list( "bob", "alice", "art", pxs( 75'000 ) ), bob_private_key ); // a new offer replaces the old one
    generate_block();
    const PXS_asset alice_before = get_pxs_balance( "alice" );
    const PXS_asset bob_before = get_pxs_balance( "bob" );
    const PXS_asset carol_before = get_pxs_balance( "carol" );
    HIVE_REQUIRE_ASSERT( push_transaction( buy( "carol", "alice", "art", pxs( 80'000 ) ), carol_private_key ),
      "a.price == copy->get_price() && \"nft_buy_price\"" );
    push_transaction( buy( "carol", "alice", "art", pxs( 75'000 ) ), carol_private_key );
    BOOST_REQUIRE( get_pxs_balance( "carol" ) == carol_before - PXS_asset( 75'000 ) );
    BOOST_REQUIRE( get_pxs_balance( "alice" ) == alice_before + PXS_asset( 7'500 ) );
    BOOST_REQUIRE( get_pxs_balance( "bob" ) == bob_before + PXS_asset( 67'500 ) );
    BOOST_REQUIRE( edition( "alice", "art" ).get_owner() == carol_id );

    generate_block();
    const auto sold = last_operation< nft_sold_operation >();
    const auto royalty = last_operation< nft_royalty_operation >();
    BOOST_REQUIRE( sold.valid() && royalty.valid() );
    BOOST_REQUIRE( sold->price == pxs( 75'000 ) && sold->royalty == pxs( 7'500 ) );
    BOOST_REQUIRE_EQUAL( royalty->creator, "alice" );
    BOOST_REQUIRE_EQUAL( royalty->seller, "bob" );
    BOOST_REQUIRE( royalty->royalty == pxs( 7'500 ) );
  }

  BOOST_TEST_MESSAGE( "--- the royalty is rounded down; a sale too small for one pays none" );
  {
    push_transaction( list( "carol", "alice", "art", pxs( 9 ) ), carol_private_key ); // 0.009 PXS, 10% = 0.0009
    generate_block();
    const PXS_asset alice_before = get_pxs_balance( "alice" );
    const PXS_asset carol_before = get_pxs_balance( "carol" );
    push_transaction( buy( "bob", "alice", "art", pxs( 9 ) ), bob_private_key );
    BOOST_REQUIRE( get_pxs_balance( "alice" ) == alice_before );
    BOOST_REQUIRE( get_pxs_balance( "carol" ) == carol_before + PXS_asset( 9 ) );
    generate_block();
    BOOST_REQUIRE( last_operation< nft_sold_operation >().valid() );
    BOOST_REQUIRE( !last_operation< nft_royalty_operation >().valid() );
  }

  BOOST_TEST_MESSAGE( "--- a buyer who cannot pay does not get the NFT" );
  {
    generate_block();
    push_transaction( list( "bob", "alice", "art", pxs( 10'000'000 ) ), bob_private_key );
    generate_block();
    HIVE_REQUIRE_THROW( push_transaction( buy( "carol", "alice", "art", pxs( 10'000'000 ) ), carol_private_key ), fc::assert_exception );
    BOOST_REQUIRE( edition( "alice", "art" ).get_owner() == bob_id );
  }
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( offer_expiration_and_unlist )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice)(bob) );
  fund( "alice", PXS_asset( 100'000 ) );
  fund( "bob", PXS_asset( 100'000 ) );
  post_comment( "alice", "art", "title", "body", "pixelart", alice_post_key );
  generate_block();
  activate();
  push_transaction( mint( "alice", "art" ), alice_private_key );
  generate_block();

  HIVE_REQUIRE_ASSERT( push_transaction( unlist( "alice", "alice", "art" ), alice_private_key ),
    "copy != nullptr && copy->has_offer() && \"nft_unlist_offer\"" );

  const fc::time_point_sec expiration = db->head_block_time() + fc::seconds( 10 * HIVE_BLOCK_INTERVAL );
  push_transaction( list( "alice", "alice", "art", pxs( 1'000 ), expiration ), alice_private_key );
  generate_blocks( expiration ); // head block time reaches the expiration: the offer is over
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "bob", "alice", "art", pxs( 1'000 ) ), bob_private_key ),
    "copy != nullptr && copy->is_listed( _now ) && \"nft_buy_listed\"" );

  // the expired offer is still recorded: the owner can remove it or simply make a new one
  BOOST_REQUIRE( edition( "alice", "art" ).has_offer() );
  HIVE_REQUIRE_ASSERT( push_transaction( unlist( "bob", "alice", "art" ), bob_private_key ),
    "owner_of( nft, copy ) == _actor.get_id() && \"nft_owner\"" );
  BOOST_REQUIRE_EQUAL( db->find_nft( account_name_type( "alice" ), "art" )->get_reserve(), 0 );
  push_transaction( unlist( "alice", "alice", "art" ), alice_private_key );
  // the copy never left alice: without its offer it is back in her reserve, without an object
  BOOST_REQUIRE( find_edition( "alice", "art" ) == nullptr );
  BOOST_REQUIRE_EQUAL( db->find_nft( account_name_type( "alice" ), "art" )->get_reserve(), 1 );
  generate_block(); // the same transaction again would be a duplicate in this block
  HIVE_REQUIRE_ASSERT( push_transaction( unlist( "alice", "alice", "art" ), alice_private_key ),
    "copy != nullptr && copy->has_offer() && \"nft_unlist_offer\"" );
  generate_block();
  push_transaction( list( "alice", "alice", "art", pxs( 2'000 ) ), alice_private_key );
  generate_block();
  push_transaction( buy( "bob", "alice", "art", pxs( 2'000 ) ), bob_private_key );
  BOOST_REQUIRE( edition( "alice", "art" ).get_owner() == bob_id );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( ten_thousand_copies )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice)(bob)(carol) );
  issue_funds( "alice", PXS_asset( 10'100'000 ) );
  fund( "bob", PXS_asset( 100'000 ) );
  fund( "carol", PXS_asset( 100'000 ) );
  post_comment( "alice", "series", "title", "body", "pixelart", alice_post_key );
  generate_block();
  activate();

  BOOST_TEST_MESSAGE( "--- 10,000 copies, all the creator's, none with an object" );
  push_transaction( mint( "alice", "series", 1000, "PIXA_LICENSE/1.0", PIXA_NFT_MAX_EDITIONS ), alice_private_key );
  const nft_object* nft = db->find_nft( account_name_type( "alice" ), "series" );
  BOOST_REQUIRE( nft != nullptr );
  BOOST_REQUIRE_EQUAL( nft->get_editions(), PIXA_NFT_MAX_EDITIONS );
  BOOST_REQUIRE_EQUAL( copy_objects(), 0u );
  BOOST_REQUIRE( db->find_nft_edition( *nft, 0 ) == nullptr ); // 0 names the NFT and its reserve, never a copy
  BOOST_REQUIRE_EQUAL( nft->get_reserve(), PIXA_NFT_MAX_EDITIONS );
  BOOST_REQUIRE_EQUAL( db->find_nft_reserve_copy( *nft ), 1 );
  generate_block();

  BOOST_TEST_MESSAGE( "--- offering the last copy creates its object, and only that one" );
  push_transaction( list( "alice", "alice", "series", pxs( 10'000 ), never, PIXA_NFT_MAX_EDITIONS ), alice_private_key );
  BOOST_REQUIRE_EQUAL( copy_objects(), 1u );
  BOOST_REQUIRE( edition( "alice", "series", PIXA_NFT_MAX_EDITIONS ).get_owner() == alice_id );
  BOOST_REQUIRE( find_edition( "alice", "series", PIXA_NFT_MAX_EDITIONS - 1 ) == nullptr );
  BOOST_REQUIRE_EQUAL( nft->get_reserve(), PIXA_NFT_MAX_EDITIONS - 1 ); // the offered copy left the reserve
  HIVE_REQUIRE_THROW( push_transaction( list( "alice", "alice", "series", pxs( 1'000 ), never, PIXA_NFT_EDITIONS_LIMIT + 1 ),
    alice_private_key ), fc::assert_exception );
  generate_block();

  BOOST_TEST_MESSAGE( "--- it sells like any copy; the others do not move" );
  push_transaction( buy( "bob", "alice", "series", pxs( 10'000 ), PIXA_NFT_MAX_EDITIONS ), bob_private_key );
  BOOST_REQUIRE( edition( "alice", "series", PIXA_NFT_MAX_EDITIONS ).get_owner() == bob_id );
  BOOST_REQUIRE_EQUAL( copy_objects(), 1u );
  generate_block();

  BOOST_TEST_MESSAGE( "--- copies are numbered up to the number minted" );
  generate_blocks( db->head_block_time() + HIVE_MIN_ROOT_COMMENT_INTERVAL + fc::seconds( HIVE_BLOCK_INTERVAL ) );
  post_comment( "alice", "trio", "title", "body", "pixelart", alice_post_key );
  generate_block();
  push_transaction( mint( "alice", "trio", 0, "PIXA_LICENSE/1.0", 3 ), alice_private_key );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", "trio", pxs( 1'000 ), never, 4 ), alice_private_key ),
    "edition >= 1 && edition <= nft.get_editions() && \"nft_edition_exists\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( give( "alice", "alice", "trio", "bob", 4 ), alice_private_key ),
    "edition >= 1 && edition <= nft.get_editions() && \"nft_edition_exists\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "bob", "alice", "trio", pxs( 1'000 ), 4 ), bob_private_key ),
    "edition >= 1 && edition <= nft.get_editions() && \"nft_edition_exists\"" );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( transfer_rules )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice)(bob)(carol) );
  fund( "alice", PXS_asset( 100'000 ) );
  fund( "carol", PXS_asset( 100'000 ) );
  post_comment( "alice", "art", "title", "body", "pixelart", alice_post_key );
  generate_block();
  activate();
  push_transaction( mint( "alice", "art", 2500, "PIXA_LICENSE/1.0", 3 ), alice_private_key );
  push_transaction( list( "alice", "alice", "art", pxs( 5'000 ) ), alice_private_key );
  generate_block();

  HIVE_REQUIRE_ASSERT( push_transaction( give( "bob", "alice", "art", "carol" ), bob_private_key ),
    "owner_of( nft, copy ) == _actor.get_id() && \"nft_owner\"" );
  HIVE_REQUIRE_THROW( push_transaction( give( "alice", "alice", "art", "alice" ), alice_private_key ), fc::assert_exception );
  HIVE_REQUIRE_THROW( push_transaction( give( "alice", "alice", "art", "nobody" ), alice_private_key ), fc::exception );
  HIVE_REQUIRE_ASSERT( push_transaction( give( "alice", "alice", "art", db->get_treasury_name() ), alice_private_key ),
    "!_db.is_treasury( a.to ) && \"nft_transfer_to_treasury\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( give( "alice", "alice", "art", "bob", 4 ), alice_private_key ),
    "edition >= 1 && edition <= nft.get_editions() && \"nft_edition_exists\"" );

  BOOST_TEST_MESSAGE( "--- a gift: no payment, so no royalty, and the offer ends" );
  const PXS_asset alice_before = get_pxs_balance( "alice" );
  push_transaction( give( "alice", "alice", "art", "bob" ), alice_private_key );
  BOOST_REQUIRE( edition( "alice", "art" ).get_owner() == bob_id );
  BOOST_REQUIRE( !edition( "alice", "art" ).has_offer() ); // the offer of the previous owner ended
  BOOST_REQUIRE( get_pxs_balance( "alice" ) == alice_before ); // a gift pays nobody
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "carol", "alice", "art", pxs( 5'000 ) ), carol_private_key ),
    "copy != nullptr && copy->is_listed( _now ) && \"nft_buy_listed\"" );
  generate_block();

  BOOST_TEST_MESSAGE( "--- giving a copy that never moved creates its object, owned by the receiver" );
  BOOST_REQUIRE( find_edition( "alice", "art", 3 ) == nullptr );
  push_transaction( give( "alice", "alice", "art", "carol", 3 ), alice_private_key );
  BOOST_REQUIRE( edition( "alice", "art", 3 ).get_owner() == carol_id );
  BOOST_REQUIRE( edition( "alice", "art", 3 ).get_acquired() == db->head_block_time() );
  generate_block();

  BOOST_TEST_MESSAGE( "--- a copy given to null is burned, even one that never moved" );
  push_transaction( give( "alice", "alice", "art", HIVE_NULL_ACCOUNT, 2 ), alice_private_key );
  const account_id_type null_id = db->get_account( HIVE_NULL_ACCOUNT ).get_id();
  BOOST_REQUIRE( edition( "alice", "art", 2 ).get_owner() == null_id );
  BOOST_REQUIRE( !edition( "alice", "art", 2 ).has_offer() );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "carol", "alice", "art", pxs( 5'000 ), 2 ), carol_private_key ),
    "copy != nullptr && copy->is_listed( _now ) && \"nft_buy_listed\"" );
  // the creator has no say over a burned copy either
  HIVE_REQUIRE_ASSERT( push_transaction( give( "alice", "alice", "art", "carol", 2 ), alice_private_key ),
    "owner_of( nft, copy ) == _actor.get_id() && \"nft_owner\"" );
  generate_block(); // the null account is cleared at the end of every block: its balances, not the copy
  BOOST_REQUIRE( edition( "alice", "art", 2 ).get_owner() == null_id );
  BOOST_REQUIRE_EQUAL( db->find_nft( account_name_type( "alice" ), "art" )->get_editions(), 3 ); // still counted
  BOOST_REQUIRE( edition( "alice", "art", 1 ).get_owner() == bob_id );
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( reserve_offer )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice)(bob)(carol) );
  fund( "alice", PXS_asset( 100'000 ) );
  fund( "bob", PXS_asset( 100'000 ) );
  fund( "carol", PXS_asset( 100'000 ) );
  post_comment( "alice", "art", "title", "body", "pixelart", alice_post_key );
  generate_block();
  activate();
  push_transaction( mint( "alice", "art", 1000, "PIXA_LICENSE/1.0", 5 ), alice_private_key ); // 10% royalty
  generate_block();
  const nft_object& nft = *db->find_nft( account_name_type( "alice" ), "art" );

  BOOST_TEST_MESSAGE( "--- at mint every copy is in the reserve, which nobody offered yet" );
  BOOST_REQUIRE_EQUAL( nft.get_reserve(), 5 );
  BOOST_REQUIRE_EQUAL( nft.get_reserve_start(), 1 );
  BOOST_REQUIRE( !nft.has_reserve_offer() );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "bob", "alice", "art", pxs( 5'000 ), 0 ), bob_private_key ),
    "nft.is_reserve_listed( _now ) && \"nft_buy_reserve_listed\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( unlist( "alice", "alice", "art", 0 ), alice_private_key ),
    "nft.has_reserve_offer() && \"nft_unlist_reserve_offer\"" );

  BOOST_TEST_MESSAGE( "--- only the creator offers the reserve, in PXS, with an expiration in the future" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "bob", "alice", "art", pxs( 5'000 ), never, 0 ), bob_private_key ),
    "nft.get_creator() == _actor.get_id() && \"nft_reserve_creator\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( unlist( "bob", "alice", "art", 0 ), bob_private_key ),
    "nft.get_creator() == _actor.get_id() && \"nft_reserve_creator\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", "art", pixa( 5'000 ), never, 0 ), alice_private_key ),
    "_db.is_nft_price_symbol( a.price.symbol ) && \"nft_list_price_symbol\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", "art", pxs( 5'000 ), db->head_block_time(), 0 ), alice_private_key ),
    "a.expiration > _now && \"nft_list_expiration\"" );

  BOOST_TEST_MESSAGE( "--- a copy offered on its own leaves the reserve" );
  push_transaction( list( "alice", "alice", "art", pxs( 7'000 ), never, 2 ), alice_private_key );
  BOOST_REQUIRE_EQUAL( nft.get_reserve(), 4 ); // copies 1, 3, 4 and 5
  BOOST_REQUIRE( !edition( "alice", "art", 2 ).has_left_creator() );
  push_transaction( list( "alice", "alice", "art", pxs( 5'000 ), never, 0 ), alice_private_key );
  BOOST_REQUIRE( nft.is_reserve_listed( db->head_block_time() ) );
  BOOST_REQUIRE( nft.get_reserve_price() == pxs( 5'000 ) );
  BOOST_REQUIRE_EQUAL( db->find_nft_reserve_copy( nft ), 1 );
  BOOST_REQUIRE_EQUAL( copy_objects(), 1u ); // the offer of the reserve creates no copy object
  generate_block();

  BOOST_TEST_MESSAGE( "--- the buyer pays the creator's price, and the creator cannot buy his own reserve" );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "bob", "alice", "art", pxs( 4'999 ), 0 ), bob_private_key ),
    "a.price == nft.get_reserve_price() && \"nft_buy_reserve_price\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "alice", "alice", "art", pxs( 5'000 ), 0 ), alice_private_key ),
    "nft.get_creator() != _actor.get_id() && \"nft_buy_reserve_own\"" );

  BOOST_TEST_MESSAGE( "--- each buyer gets the lowest-numbered copy of the reserve; the creator gets the whole price" );
  {
    const PXS_asset alice_before = get_pxs_balance( "alice" );
    const PXS_asset bob_before = get_pxs_balance( "bob" );
    push_transaction( buy( "bob", "alice", "art", pxs( 5'000 ), 0 ), bob_private_key );
    BOOST_REQUIRE( get_pxs_balance( "bob" ) == bob_before - PXS_asset( 5'000 ) );
    BOOST_REQUIRE( get_pxs_balance( "alice" ) == alice_before + PXS_asset( 5'000 ) ); // no royalty to herself
    BOOST_REQUIRE( edition( "alice", "art", 1 ).get_owner() == bob_id );
    BOOST_REQUIRE( edition( "alice", "art", 1 ).has_left_creator() );
    BOOST_REQUIRE( edition( "alice", "art", 1 ).get_acquired() == db->head_block_time() );
    BOOST_REQUIRE_EQUAL( nft.get_reserve(), 3 );

    generate_block();
    const auto sold = last_operation< nft_sold_operation >();
    BOOST_REQUIRE( sold.valid() );
    BOOST_REQUIRE_EQUAL( sold->seller, "alice" );
    BOOST_REQUIRE_EQUAL( sold->buyer, "bob" );
    BOOST_REQUIRE_EQUAL( sold->edition, 1 ); // the copy that was sold, not 0
    BOOST_REQUIRE( sold->price == pxs( 5'000 ) && sold->royalty == pxs( 0 ) );
    BOOST_REQUIRE( !last_operation< nft_royalty_operation >().valid() );
  }
  // copy 2 has its own offer, so the next buyer of the reserve gets copy 3
  push_transaction( buy( "carol", "alice", "art", pxs( 5'000 ), 0 ), carol_private_key );
  BOOST_REQUIRE( edition( "alice", "art", 3 ).get_owner() == carol_id );
  BOOST_REQUIRE( edition( "alice", "art", 2 ).get_owner() == alice_id );
  BOOST_REQUIRE_EQUAL( nft.get_reserve(), 2 );
  BOOST_REQUIRE_EQUAL( db->find_nft_reserve_copy( nft ), 4 );
  generate_block();

  BOOST_TEST_MESSAGE( "--- withdrawing the offer of a copy that never left the creator puts it back in the reserve" );
  push_transaction( unlist( "alice", "alice", "art", 2 ), alice_private_key );
  BOOST_REQUIRE( find_edition( "alice", "art", 2 ) == nullptr );
  BOOST_REQUIRE_EQUAL( nft.get_reserve(), 3 );
  BOOST_REQUIRE_EQUAL( db->find_nft_reserve_copy( nft ), 2 );
  push_transaction( buy( "bob", "alice", "art", pxs( 5'000 ), 0 ), bob_private_key );
  BOOST_REQUIRE( edition( "alice", "art", 2 ).get_owner() == bob_id );
  BOOST_REQUIRE_EQUAL( nft.get_reserve(), 2 ); // copies 4 and 5
  generate_block();

  BOOST_TEST_MESSAGE( "--- a copy that left and came back stays out of the reserve" );
  push_transaction( give( "bob", "alice", "art", "alice", 1 ), bob_private_key );
  push_transaction( list( "alice", "alice", "art", pxs( 9'000 ), never, 1 ), alice_private_key );
  generate_block();
  push_transaction( unlist( "alice", "alice", "art", 1 ), alice_private_key );
  BOOST_REQUIRE( edition( "alice", "art", 1 ).get_owner() == alice_id ); // keeps its object
  BOOST_REQUIRE( edition( "alice", "art", 1 ).has_left_creator() );
  BOOST_REQUIRE( !edition( "alice", "art", 1 ).has_offer() );
  BOOST_REQUIRE_EQUAL( nft.get_reserve(), 2 );
  generate_block();

  BOOST_TEST_MESSAGE( "--- the reserve offer expires like any offer, and can be withdrawn" );
  {
    const fc::time_point_sec expiration = db->head_block_time() + fc::seconds( 10 * HIVE_BLOCK_INTERVAL );
    push_transaction( list( "alice", "alice", "art", pxs( 6'000 ), expiration, 0 ), alice_private_key ); // replaces the offer
    BOOST_REQUIRE( nft.get_reserve_price() == pxs( 6'000 ) );
    generate_blocks( expiration );
    HIVE_REQUIRE_ASSERT( push_transaction( buy( "carol", "alice", "art", pxs( 6'000 ), 0 ), carol_private_key ),
      "nft.is_reserve_listed( _now ) && \"nft_buy_reserve_listed\"" );
    BOOST_REQUIRE( nft.has_reserve_offer() );
    push_transaction( unlist( "alice", "alice", "art", 0 ), alice_private_key );
    BOOST_REQUIRE( !nft.has_reserve_offer() );
    BOOST_REQUIRE_EQUAL( nft.get_reserve(), 2 ); // withdrawing the offer moves no copy
  }
  generate_block();

  BOOST_TEST_MESSAGE( "--- the offer ends with the last copy of the reserve" );
  push_transaction( list( "alice", "alice", "art", pxs( 5'000 ), never, 0 ), alice_private_key );
  generate_block();
  push_transaction( buy( "bob", "alice", "art", pxs( 5'000 ), 0 ), bob_private_key );
  push_transaction( buy( "carol", "alice", "art", pxs( 5'000 ), 0 ), carol_private_key );
  BOOST_REQUIRE( edition( "alice", "art", 4 ).get_owner() == bob_id );
  BOOST_REQUIRE( edition( "alice", "art", 5 ).get_owner() == carol_id );
  BOOST_REQUIRE_EQUAL( nft.get_reserve(), 0 );
  BOOST_REQUIRE( !nft.has_reserve_offer() );
  BOOST_REQUIRE_EQUAL( db->find_nft_reserve_copy( nft ), 0 );
  BOOST_REQUIRE_EQUAL( copy_objects(), 5u );
  generate_block();
  HIVE_REQUIRE_ASSERT( push_transaction( buy( "carol", "alice", "art", pxs( 5'000 ), 0 ), carol_private_key ),
    "nft.is_reserve_listed( _now ) && \"nft_buy_reserve_listed\"" );
  HIVE_REQUIRE_ASSERT( push_transaction( list( "alice", "alice", "art", pxs( 5'000 ), never, 0 ), alice_private_key ),
    "nft.get_reserve() > 0 && \"nft_list_reserve\"" );

  BOOST_TEST_MESSAGE( "--- a copy bought from the reserve resells with the royalty" );
  push_transaction( list( "bob", "alice", "art", pxs( 20'000 ), never, 4 ), bob_private_key );
  generate_block();
  {
    const PXS_asset alice_before = get_pxs_balance( "alice" );
    push_transaction( buy( "carol", "alice", "art", pxs( 20'000 ), 4 ), carol_private_key );
    BOOST_REQUIRE( get_pxs_balance( "alice" ) == alice_before + PXS_asset( 2'000 ) );
  }
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( voting_bar )
{ try {
  ACTORS( PIXA_asset( 1'000'000 ), (alice)(bob)(carol)(dave)(eve) );
  generate_block(); // the same vesting transfers as ACTORS made would be duplicate transactions in its block
  vest( "alice", PIXA_asset( 1'000'000 ) );
  vest( "bob", PIXA_asset( 1'000'000 ) );
  vest( "eve", PIXA_asset( 1'000'000 ) );
  post_comment( "carol", "one", "title", "body", "pixelart", carol_post_key );
  post_comment( "dave", "two", "title", "body", "pixelart", dave_post_key );
  post_comment( "eve", "three", "title", "body", "pixelart", eve_post_key );
  generate_block();

  BOOST_TEST_MESSAGE( "--- before the hardfork: 2% per full vote, 5-day refill, 5-day delegation return" );
  const dynamic_global_property_object& gpo = db->get_dynamic_global_properties();
  BOOST_REQUIRE_EQUAL( gpo.get_voting_mana_regeneration_seconds(), HIVE_VOTING_MANA_REGENERATION_SECONDS );
  BOOST_REQUIRE_EQUAL( gpo.get_vote_power_reserve_rate(), HIVE_REDUCED_VOTE_POWER_RATE );
  BOOST_REQUIRE_EQUAL( gpo.get_delegation_return_period(), HIVE_DELEGATION_RETURN_PERIOD_HF20 );

  // a delegation removed before the fork keeps its return date
  const VEST_asset lent( db->get_account( "eve" ).get_vesting().amount / 10 );
  delegate_vest( "eve", "dave", lent, eve_private_key );
  generate_block();
  delegate_vest( "eve", "dave", VEST_asset( 0 ), eve_private_key );
  const fc::time_point_sec early_return = db->head_block_time() + fc::seconds( HIVE_DELEGATION_RETURN_PERIOD_HF20 );
  generate_block();

  // bob emptied his bar two days ago and did nothing since
  const uint32_t emptied = db->head_block_time().sec_since_epoch() - 2 * 24 * 3600;
  db_plugin->debug_update( [=]( database& chain )
  {
    chain.modify( chain.get_account( "bob" ), [&]( account_object& a )
    {
      a.voting_manabar.current_mana = 0;
      a.voting_manabar.last_update_time = emptied;
    } );
  } );
  generate_block();

  activate();
  BOOST_TEST_MESSAGE( "--- after it: 3% per full vote, 6 days 16 hours to refill, 7 days to return lent stake" );
  BOOST_REQUIRE( db->has_hardfork( HIVE_HARDFORK_1_32_VOTING_BAR ) );
  BOOST_REQUIRE_EQUAL( gpo.get_voting_mana_regeneration_seconds(), 576'000u );
  BOOST_REQUIRE_EQUAL( gpo.get_vote_power_reserve_rate(), 5u );
  BOOST_REQUIRE_EQUAL( gpo.get_delegation_return_period(), 604'800u );
  BOOST_REQUIRE_EQUAL( HIVE_RC_REGEN_TIME, 5 * 24 * 3600 ); // resource credits keep their 5 days

  BOOST_TEST_MESSAGE( "--- the fork settled every bar at the old rate first" );
  {
    const account_object& bob = db->get_account( "bob" );
    const int64_t max_mana = bob.get_effective_vesting_shares().get_amount();
    const uint32_t settled = bob.voting_manabar.last_update_time;
    BOOST_REQUIRE_GT( settled, emptied ); // brought up to date by the hardfork, not by an action of bob
    const int64_t at_old_rate = fc::uint128_to_int64( ( fc::uint128_t( max_mana ) * ( settled - emptied ) ) / HIVE_VOTING_MANA_REGENERATION_SECONDS );
    BOOST_REQUIRE_EQUAL( bob.voting_manabar.current_mana, at_old_rate ); // about 40%, not 30%
  }

  BOOST_TEST_MESSAGE( "--- delegations: removed before the fork keep 5 days, removed after take 7" );
  {
    const auto& returns = db->get_index< vesting_delegation_expiration_index, by_id >();
    BOOST_REQUIRE_EQUAL( returns.size(), 1u );
    BOOST_REQUIRE( returns.begin()->get_expiration_time() == early_return );
    delegate_vest( "eve", "dave", lent, eve_private_key );
    generate_block();
    delegate_vest( "eve", "dave", VEST_asset( 0 ), eve_private_key );
    const fc::time_point_sec late_return = db->head_block_time() + fc::seconds( PIXA_HF32_DELEGATION_RETURN_PERIOD );
    BOOST_REQUIRE_EQUAL( returns.size(), 2u );
    BOOST_REQUIRE( ( ++returns.begin() )->get_expiration_time() == late_return );
    generate_block();
  }

  // the cost of a vote, computed the way hf20_vote_evaluator does
  const auto cost = [&]( int64_t bar, int16_t weight ) -> int64_t
  {
    const fc::uint128_t used = ( fc::uint128_t( bar ) * weight * 60 * 60 * 24 ) / HIVE_100_PERCENT;
    const int64_t denom = int64_t( gpo.get_vote_power_reserve_rate() ) * gpo.get_voting_mana_regeneration_seconds();
    return fc::uint128_to_int64( ( used + denom - 1 ) / denom );
  };
  const account_object& alice = db->get_account( "alice" );
  const int64_t bar = alice.get_effective_vesting_shares().get_amount();
  const int64_t full = cost( bar, HIVE_100_PERCENT );

  BOOST_TEST_MESSAGE( "--- a full vote costs 3% of the bar: 33 from a full bar, 8 downvotes from a full pool" );
  BOOST_REQUIRE_EQUAL( full, ( bar * 3 + 99 ) / 100 );
  BOOST_REQUIRE_EQUAL( bar / full, 33 );
  BOOST_REQUIRE_EQUAL( ( bar * gpo.get_downvote_pool_percent() / HIVE_100_PERCENT ) / full, 8 );
  BOOST_REQUIRE_EQUAL( cost( bar, HIVE_1_PERCENT ), ( bar * 3 + 9'999 ) / 10'000 );
  BOOST_REQUIRE_GT( cost( bar, 1 ), 0 ); // the smallest vote is still a vote

  {
    const uint32_t since = db->head_block_time().sec_since_epoch();
    db_plugin->debug_update( [=]( database& chain )
    {
      chain.modify( chain.get_account( "alice" ), [&]( account_object& a )
      {
        a.voting_manabar.current_mana = bar; // a full bar
        a.voting_manabar.last_update_time = since;
      } );
    } );
    generate_block();
  }
  vote( "carol", "one", "alice", HIVE_100_PERCENT, alice_post_key );
  {
    const comment post = db->get_comment( account_name_type( "carol" ), std::string( "one" ) );
    const comment_cashout_object* cashout = db->find_comment_cashout( *post );
    BOOST_REQUIRE( cashout != nullptr );
    BOOST_REQUIRE_EQUAL( cashout->get_net_rshares(), full - PIXA_HF30_VOTE_DUST_THRESHOLD );
    BOOST_REQUIRE_EQUAL( alice.voting_manabar.current_mana, bar - full );
  }
  generate_block();

  BOOST_TEST_MESSAGE( "--- an empty bar refills 15% a day" );
  {
    const uint32_t since = db->head_block_time().sec_since_epoch() - 24 * 3600;
    db_plugin->debug_update( [=]( database& chain )
    {
      chain.modify( chain.get_account( "alice" ), [&]( account_object& a )
      {
        a.voting_manabar.current_mana = 0;
        a.voting_manabar.last_update_time = since;
      } );
    } );
    generate_block();
    const uint32_t now = db->head_block_time().sec_since_epoch();
    vote( "dave", "two", "alice", HIVE_1_PERCENT, alice_post_key );
    const int64_t refilled = fc::uint128_to_int64( ( fc::uint128_t( bar ) * ( now - since ) ) / 576'000 );
    BOOST_REQUIRE_EQUAL( alice.voting_manabar.current_mana, refilled - cost( bar, HIVE_1_PERCENT ) );
    BOOST_REQUIRE_GE( refilled, bar * 15 / 100 ); // a day and a block or two: just above 15%
    BOOST_REQUIRE_LT( refilled, bar * 16 / 100 );
  }
  generate_block();

  BOOST_TEST_MESSAGE( "--- a vote the bar cannot pay for is rejected" );
  {
    const uint32_t since = db->head_block_time().sec_since_epoch();
    db_plugin->debug_update( [=]( database& chain )
    {
      chain.modify( chain.get_account( "alice" ), [&]( account_object& a )
      {
        a.voting_manabar.current_mana = full; // exactly one full vote left
        a.voting_manabar.last_update_time = since;
      } );
    } );
    generate_block();
    vote( "eve", "three", "alice", HIVE_100_PERCENT, alice_post_key );
    BOOST_REQUIRE_LT( alice.voting_manabar.current_mana, full );
    // raising the 1% vote on "two" to a full one needs a full vote's mana again
    HIVE_REQUIRE_THROW( vote( "dave", "two", "alice", HIVE_100_PERCENT, alice_post_key ), fc::exception );
  }
  generate_block();

  BOOST_TEST_MESSAGE( "--- a downvote takes the pool first, then the bar" );
  {
    const uint32_t since = db->head_block_time().sec_since_epoch();
    const int64_t pool_max = fc::uint128_to_int64( ( fc::uint128_t( bar ) * gpo.get_downvote_pool_percent() ) / HIVE_100_PERCENT );
    db_plugin->debug_update( [=]( database& chain )
    {
      chain.modify( chain.get_account( "alice" ), [&]( account_object& a )
      {
        a.voting_manabar.current_mana = bar;
        a.voting_manabar.last_update_time = since;
        a.downvote_manabar.current_mana = bar / 100; // what is left after 8 full downvotes
        a.downvote_manabar.last_update_time = since;
      } );
    } );
    generate_block();
    const uint32_t now = db->head_block_time().sec_since_epoch();
    vote( "dave", "two", "alice", -HIVE_100_PERCENT, alice_post_key );
    const int64_t pool = std::min( pool_max,
      bar / 100 + fc::uint128_to_int64( ( fc::uint128_t( pool_max ) * ( now - since ) ) / 576'000 ) );
    BOOST_REQUIRE_EQUAL( alice.downvote_manabar.current_mana, 0 );
    BOOST_REQUIRE_EQUAL( alice.voting_manabar.current_mana, bar - ( full - pool ) ); // about 2% from the bar
  }
  validate_database();
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( wire_format )
{ try {
  nft_mint action;
  action.permlink = "art";
  action.royalty = 1000;
  action.license = "PIXA_LICENSE/1.0";
  action.fee = asset( 1'000, PXS_SYMBOL );
  nft_operation minted;
  minted.account = "alice";
  minted.action = action;
  const operation op = minted;

  BOOST_TEST_MESSAGE( "--- binary: the tag 37, then the fields in declaration order" );
  const std::vector< char > packed = fc::raw::pack_to_vector( op );
  BOOST_REQUIRE_EQUAL( uint8_t( packed[0] ), 37 );
  operation unpacked;
  fc::raw::unpack_from_vector( packed, unpacked );
  BOOST_REQUIRE( unpacked.which() == operation::tag< nft_operation >::value );
  BOOST_REQUIRE_EQUAL( unpacked.get< nft_operation >().account, "alice" );
  BOOST_REQUIRE( unpacked.get< nft_operation >().action.get< nft_mint >().fee == asset( 1'000, PXS_SYMBOL ) );
  BOOST_REQUIRE( fc::raw::pack_to_vector( unpacked ) == packed );

  BOOST_TEST_MESSAGE( "--- JSON: {type, value} at both levels, as indexers read it" );
  const fc::variant json = fc::json::from_string( fc::json::to_string( op ), fc::json::format_validation_mode::full );
  BOOST_REQUIRE_EQUAL( json[ "type" ].as_string(), "nft_operation" );
  BOOST_REQUIRE_EQUAL( json[ "value" ][ "account" ].as_string(), "alice" );
  BOOST_REQUIRE_EQUAL( json[ "value" ][ "action" ][ "type" ].as_string(), "nft_mint" );
  BOOST_REQUIRE_EQUAL( json[ "value" ][ "action" ][ "value" ][ "royalty" ].as_uint64(), 1000u );
  operation from_json;
  fc::from_variant( json, from_json );
  BOOST_REQUIRE( fc::raw::pack_to_vector( from_json ) == packed );

  BOOST_TEST_MESSAGE( "--- stateless validation" );
  operation_validate( op );
  {
    nft_operation bad = minted;
    bad.action.get< nft_mint >().fee = asset( 1'000, PIXA_SYMBOL ); // the fee is paid in PXS
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    bad = minted;
    bad.action.get< nft_mint >().editions = 0;
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    bad.action.get< nft_mint >().editions = PIXA_NFT_EDITIONS_LIMIT + 1;
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    bad.action.get< nft_mint >().editions = PIXA_NFT_EDITIONS_LIMIT; // 10,000
    bad.validate();
    bad.action.get< nft_mint >().license = longest_license(); // 72 KiB of text
    bad.validate();
    bad.action.get< nft_mint >().license += "z";
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    bad.action.get< nft_mint >().license = std::string( "\xC3", 1 ); // not UTF-8
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    nft_list offer;
    offer.author = "alice";
    offer.permlink = "art";
    offer.price = asset( 0, PXS_SYMBOL ); // an offer needs a price
    bad.action = offer;
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    offer.price = asset( 1'000'000, VESTS_SYMBOL );
    bad.action = offer;
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    offer.price = asset( 1'000, PIXA_SYMBOL ); // the format takes PIXA; consensus takes PXS only (list_buy_and_royalty)
    bad.action = offer;
    bad.validate();
    offer.edition = 0; // 0 is the reserve: it can be offered, withdrawn and bought
    bad.action = offer;
    bad.validate();
    offer.edition = PIXA_NFT_EDITIONS_LIMIT;
    bad.action = offer;
    bad.validate();
    offer.edition = PIXA_NFT_EDITIONS_LIMIT + 1;
    bad.action = offer;
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    nft_unlist withdraw;
    withdraw.author = "alice";
    withdraw.permlink = "art";
    withdraw.edition = 0;
    bad.action = withdraw;
    bad.validate();
    nft_buy purchase;
    purchase.author = "alice";
    purchase.permlink = "art";
    purchase.edition = 0;
    purchase.price = asset( 1'000, PXS_SYMBOL );
    bad.account = "bob";
    bad.action = purchase;
    bad.validate();
    nft_transfer gift;
    gift.author = "alice";
    gift.permlink = "art";
    gift.edition = 0; // but not given: a gift is always one numbered copy
    gift.to = "carol";
    bad.action = gift;
    HIVE_REQUIRE_THROW( bad.validate(), fc::assert_exception );
    gift.edition = 1;
    bad.action = gift;
    bad.validate();
  }

  BOOST_TEST_MESSAGE( "--- a mint with the longest license fits in one transaction" );
  {
    nft_operation longest = minted;
    longest.action.get< nft_mint >().license = longest_license();
    signed_transaction tx;
    tx.operations.push_back( longest );
    BOOST_REQUIRE_LT( fc::raw::pack_size( tx ) + 2 * 65, size_t( HIVE_MAX_TRANSACTION_SIZE ) ); // with two signatures
  }

  BOOST_TEST_MESSAGE( "--- authority and impact" );
  flat_set< account_name_type > active, owner, posting, witness;
  std::vector< authority > other;
  operation_get_required_authorities( op, active, owner, posting, witness, other );
  BOOST_REQUIRE( active == flat_set< account_name_type >{ "alice" } );
  BOOST_REQUIRE( owner.empty() && posting.empty() && witness.empty() && other.empty() );

  const auto balances = hive::app::operation_get_impacted_balances( op, true );
  BOOST_REQUIRE_EQUAL( balances.size(), 2u ); // the minter pays the fee, null receives it
  const auto sale = hive::app::operation_get_impacted_balances(
    operation( nft_sold_operation( "bob", "carol", "alice", "art", 1, pxs( 75'000 ), pxs( 7'500 ) ) ), true );
  BOOST_REQUIRE_EQUAL( sale.size(), 2u ); // the buyer pays the price, the seller gets it less the royalty
  const auto royalty = hive::app::operation_get_impacted_balances(
    operation( nft_royalty_operation( "alice", "bob", "carol", "art", 1, pxs( 7'500 ) ) ), true );
  BOOST_REQUIRE_EQUAL( royalty.size(), 1u );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_CASE( resource_credits )
{ try {
  // a mint stores the same state whatever the number of copies and the length of the license;
  // the license costs execution time, per byte; offering or giving a copy may create its object
  signed_transaction small, large, many, offer, gift;
  small.operations.push_back( mint( "alice", "a", 0, "" ) );
  large.operations.push_back( mint( "alice", "a", 0, longest_license() ) );
  many.operations.push_back( mint( "alice", "a", 0, "", PIXA_NFT_MAX_EDITIONS ) );
  offer.operations.push_back( list( "alice", "alice", "a", pxs( 1'000 ) ) );
  gift.operations.push_back( give( "alice", "alice", "a", "bob" ) );
  for( signed_transaction* tx : { &small, &large, &many, &offer, &gift } )
    tx->set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

  count_resources_result small_cost, large_cost, many_cost, offer_cost, gift_cost;
  count_resources( small, fc::raw::pack_size( small ), small_cost, db->head_block_time() );
  count_resources( large, fc::raw::pack_size( large ), large_cost, db->head_block_time() );
  count_resources( many, fc::raw::pack_size( many ), many_cost, db->head_block_time() );
  count_resources( offer, fc::raw::pack_size( offer ), offer_cost, db->head_block_time() );
  count_resources( gift, fc::raw::pack_size( gift ), gift_cost, db->head_block_time() );

  BOOST_REQUIRE_EQUAL( large_cost[ resource_state_bytes ], small_cost[ resource_state_bytes ] );
  BOOST_REQUIRE_GT( large_cost[ resource_execution_time ], small_cost[ resource_execution_time ] );
  BOOST_REQUIRE_GT( large_cost[ resource_history_bytes ], small_cost[ resource_history_bytes ] );

  BOOST_REQUIRE_EQUAL( fc::raw::pack_size( many ), fc::raw::pack_size( small ) ); // 9,999 more copies
  BOOST_REQUIRE_EQUAL( many_cost[ resource_state_bytes ], small_cost[ resource_state_bytes ] );
  BOOST_REQUIRE_EQUAL( many_cost[ resource_execution_time ], small_cost[ resource_execution_time ] );

  BOOST_REQUIRE_GT( offer_cost[ resource_state_bytes ], 0 ); // the object of the copy, if it has none yet
  BOOST_REQUIRE_EQUAL( gift_cost[ resource_state_bytes ], offer_cost[ resource_state_bytes ] );
  BOOST_REQUIRE_GT( small_cost[ resource_state_bytes ], offer_cost[ resource_state_bytes ] );
  BOOST_REQUIRE_GT( small_cost[ resource_execution_time ], offer_cost[ resource_execution_time ] );

  // the offer of the reserve (copy 0) changes the NFT only; a sale from it creates the copy's object
  signed_transaction reserve_offer, withdraw, reserve_sale;
  reserve_offer.operations.push_back( list( "alice", "alice", "a", pxs( 1'000 ), never, 0 ) );
  withdraw.operations.push_back( unlist( "alice", "alice", "a" ) );
  reserve_sale.operations.push_back( buy( "bob", "alice", "a", pxs( 1'000 ), 0 ) );
  for( signed_transaction* tx : { &reserve_offer, &withdraw, &reserve_sale } )
    tx->set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  count_resources_result reserve_offer_cost, withdraw_cost, reserve_sale_cost;
  count_resources( reserve_offer, fc::raw::pack_size( reserve_offer ), reserve_offer_cost, db->head_block_time() );
  count_resources( withdraw, fc::raw::pack_size( withdraw ), withdraw_cost, db->head_block_time() );
  count_resources( reserve_sale, fc::raw::pack_size( reserve_sale ), reserve_sale_cost, db->head_block_time() );
  BOOST_REQUIRE_EQUAL( reserve_offer_cost[ resource_state_bytes ], withdraw_cost[ resource_state_bytes ] ); // the transaction only
  BOOST_REQUIRE_EQUAL( reserve_sale_cost[ resource_state_bytes ], offer_cost[ resource_state_bytes ] );

  // a removed delegation is held for the return period in force: 5 days, then 7 from hardfork 32
  signed_transaction removal;
  delegate_vesting_shares_operation undelegate;
  undelegate.delegator = "alice";
  undelegate.delegatee = "bob";
  undelegate.vesting_shares = asset( 0, VESTS_SYMBOL );
  removal.operations.push_back( undelegate );
  removal.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  count_resources_result five_days, seven_days;
  count_resources( removal, fc::raw::pack_size( removal ), five_days, db->head_block_time(), true, HIVE_DELEGATION_RETURN_PERIOD_HF20 );
  count_resources( removal, fc::raw::pack_size( removal ), seven_days, db->head_block_time(), true, PIXA_HF32_DELEGATION_RETURN_PERIOD );
  const state_object_size_info sizes;
  BOOST_REQUIRE_EQUAL( sizes.vesting_delegation_expiration_hourly_size * 5 * 24, sizes.vesting_delegation_expiration_object_size );
  BOOST_REQUIRE_EQUAL( seven_days[ resource_state_bytes ] - five_days[ resource_state_bytes ],
    sizes.vesting_delegation_expiration_hourly_size * 2 * 24 );
} FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()

#endif

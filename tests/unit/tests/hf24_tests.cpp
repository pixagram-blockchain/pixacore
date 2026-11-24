#ifdef IS_TEST_NET

#include <boost/test/unit_test.hpp>

#include <hive/chain/hive_fwd.hpp>

#include <hive/protocol/exceptions.hpp>
#include <hive/protocol/hardfork.hpp>
#include <hive/protocol/dhf_operations.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/database_exceptions.hpp>
#include <hive/chain/detail/state/convert_request_object.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object.hpp>
#include <hive/chain/detail/state/escrow_object.hpp>
#include <hive/chain/detail/state/savings_withdraw_object.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object.hpp>
#include <hive/chain/detail/state/feed_history_object.hpp>
#include <hive/chain/detail/state/limit_order_object.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object.hpp>
#include <hive/chain/detail/state/reward_fund_object.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object.hpp>

#include <hive/chain/util/reward.hpp>
#include <hive/chain/detail/state/comment_object.hpp>
#include <hive/chain/detail/state/witness_objects.hpp>
#include <hive/chain/detail/state/hardfork_property_object.hpp>
#include <hive/chain/detail/state/global_property_object.hpp>
// Multiindex headers for index type definitions
#include <hive/chain/detail/state/comment_object_multiindex.hpp>
#include <hive/chain/detail/state/transaction_object_multiindex.hpp>
#include <hive/chain/detail/state/dhf_objects_multiindex.hpp>
#include <hive/chain/detail/state/block_summary_object_multiindex.hpp>
#include <hive/chain/detail/state/hardfork_property_object_multiindex.hpp>
#include <hive/chain/detail/state/witness_objects_multiindex.hpp>
#include <hive/chain/detail/state/global_property_object_multiindex.hpp>
#include <hive/chain/detail/state/feed_history_object_multiindex.hpp>
#include <hive/chain/detail/state/limit_order_object_multiindex.hpp>
#include <hive/chain/detail/state/decline_voting_rights_request_object_multiindex.hpp>
#include <hive/chain/detail/state/reward_fund_object_multiindex.hpp>
#include <hive/chain/detail/state/recurrent_transfer_object_multiindex.hpp>
#include <hive/chain/detail/state/convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/collateralized_convert_request_object_multiindex.hpp>
#include <hive/chain/detail/state/escrow_object_multiindex.hpp>
#include <hive/chain/detail/state/savings_withdraw_object_multiindex.hpp>
#include <hive/chain/detail/state/liquidity_reward_balance_object_multiindex.hpp>
#include <hive/chain/detail/state/withdraw_vesting_route_object_multiindex.hpp>

#include <fc/macros.hpp>
#include <fc/crypto/digest.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace hive;
using namespace hive::chain;
using namespace hive::protocol;
using fc::string;


BOOST_FIXTURE_TEST_SUITE( hf24_tests, hf24_database_fixture )

BOOST_AUTO_TEST_CASE( blocked_operations )
{
  try
  {
    BOOST_TEST_MESSAGE( "Even after HF24 steem.dao is considered a treasury account" );

    ACTORS( DEFAULT_VESTING, ( alice ) );
    generate_block();
    issue_funds( "alice", PIXA_asset( 10'000 ) );
    issue_funds( "alice", PXS_asset( 10'000 ) );
    generate_block();

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    //check vesting of non-PXS
    {
      transfer_to_vesting_operation op;
      op.from = "alice";
      op.to = OBSOLETE_TREASURY_ACCOUNT;
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );
    }
    tx.clear();

    //check withdraw vesting route
    {
      set_withdraw_vesting_route_operation op;
      op.from_account = "alice";
      op.to_account = OBSOLETE_TREASURY_ACCOUNT;
      op.percent = 50 * HIVE_1_PERCENT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to_account = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );
    }
    tx.clear();

    //check transfer to savings
    {
      transfer_to_savings_operation op;
      op.from = "alice";
      op.to = OBSOLETE_TREASURY_ACCOUNT;
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.amount = ASSET( "1.000 TBD" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = "alice";
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key );
      BOOST_REQUIRE_EQUAL( get_pixa_savings( "alice" ), PIXA_asset( 1'000 ) );
    }
    tx.clear();

    //check transfer from savings of non-PXS
    {
      transfer_from_savings_operation op;
      op.from = "alice";
      op.to = OBSOLETE_TREASURY_ACCOUNT;
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );

      tx.clear();
      op.to = NEW_HIVE_TREASURY_ACCOUNT;
      tx.operations.push_back( op );
      BOOST_REQUIRE_THROW( push_transaction( tx, alice_private_key ), fc::assert_exception );
    }
    tx.clear();

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( comment_beneficiary )
{
  try
  {
    BOOST_TEST_MESSAGE( "After HF24 steem.dao as comment beneficiary gives directly to new treasury account" );

    ACTORS( PIXA_asset( 20'000 ), ( alice ) ); // give more vests to clear HIVE_VOTE_DUST_THRESHOLD
    generate_block();

    db_plugin->debug_update( []( database& db )
    {
      db.modify( db.get_dynamic_global_properties(), []( dynamic_global_property_object& gpo )
      {
        gpo.set_proposal_fund_percent( 0 );
      } );
    } );
    fund( "alice", PIXA_asset( 10'000 ) );
    fund( "alice", PXS_asset( 10'000 ) );
    generate_block();

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );

    {
      comment_operation comment;
      comment.author = "alice";
      comment.permlink = "test";
      comment.parent_permlink = "test";
      comment.title = "test";
      comment.body = "Hello world";
      tx.operations.push_back( comment );
      push_transaction( tx, alice_post_key );
    }
    tx.clear();

    {
      comment_options_operation op;
      comment_payout_beneficiaries b;
      b.beneficiaries.push_back( beneficiary_route_type( OBSOLETE_TREASURY_ACCOUNT, HIVE_100_PERCENT ) );
      op.author = "alice";
      op.permlink = "test";
      op.allow_curation_rewards = false;
      op.extensions.insert( b );
      tx.operations.push_back( op );
      push_transaction( tx, alice_post_key );
    }
    tx.clear();

    {
      vote_operation vote;
      vote.author = "alice";
      vote.permlink = "test";
      vote.voter = "alice";
      vote.weight = HIVE_100_PERCENT;
      tx.operations.push_back( vote );
      push_transaction( tx, alice_post_key );
    }
    tx.clear();

    PXS_asset initial_treasury_balance = db->get_treasury().get_pxs_balance();
    ilog( "initial treasury balance = ${b}", ( "b", initial_treasury_balance ) );
    generate_blocks( db->find_comment_cashout( *db->get_comment( "alice", string( "test" ) ) )->get_cashout_time() );
    ilog( "treasury balance after cashout = ${b}", ( "b", db->get_treasury().get_pxs_balance() ) );
    BOOST_REQUIRE_EQUAL( get_pxs_balance( OBSOLETE_TREASURY_ACCOUNT ), PXS_asset( 0 ) );
    BOOST_REQUIRE_EQUAL( db->get_treasury().get_pxs_balance(), PXS_asset( 9011 ) + initial_treasury_balance );

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( consolidate_balance )
{
  try
  {
    BOOST_TEST_MESSAGE( "After HF24 even if steem.dao gets some funds they will be transfered to new treasury account" );
    generate_block();
    
    //instead of trying to find a way to fund various balances of steem.dao, just write to them directly
    VEST_asset vested_3, vested_7;
    db_plugin->debug_update( [&]( database& db )
    {
      auto& dgpo = db.get_dynamic_global_properties();

      temp_PIXA_balance pixa_balance;
      temp_PXS_balance pxs_balance;
      db.modify( dgpo, [&]( dynamic_global_property_object& gpo )
      {
        gpo.set_proposal_fund_percent( 0 );
        pixa_balance = gpo.issue_PIXA( PIXA_asset( 20'000 ) );
        pxs_balance = gpo.issue_PXS( PXS_asset( 10'000 ), db.get_pxs_price() );
      } );

      vested_3 = PIXA_asset( 3'000 ) * dgpo.get_vesting_share_price();
      vested_7 = PIXA_asset( 7'000 ) * dgpo.get_vesting_share_price();
      auto& old_treasury = db.get_account( OBSOLETE_TREASURY_ACCOUNT );
      {
        temp_PIXA_balance to_vest;
        to_vest.transfer_from( pixa_balance, PIXA_asset( 7'000 ) );
        db.create_vesting( old_treasury, to_vest );
      }
      {
        temp_PIXA_balance to_vest;
        to_vest.transfer_from( pixa_balance, PIXA_asset( 3'000 ) );
        db.create_vesting( old_treasury, to_vest, true );
      }
      db.modify( old_treasury, [&]( account_object& t )
      {
        t.access_pixa_balance().transfer_from( pixa_balance, PIXA_asset( 5'000 ) );
        t.access_pixa_savings().transfer_from( pixa_balance, PIXA_asset( 3'000 ) );
        t.access_pixa_rewards().transfer_from( pixa_balance, PIXA_asset( 2'000 ) );
        t.access_pxs_balance().transfer_from( pxs_balance, PXS_asset( 5'000 ) );
        t.access_pxs_savings().transfer_from( pxs_balance, PXS_asset( 3'000 ) );
        t.access_pxs_rewards().transfer_from( pxs_balance, PXS_asset( 2'000 ) );
      } );
    } );
    database_fixture::validate_database();
    {
      auto& old_treasury = db->get_account( OBSOLETE_TREASURY_ACCOUNT );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pixa_balance(), PIXA_asset( 5000 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pixa_savings(), PIXA_asset( 3000 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pixa_rewards(), PIXA_asset( 2000 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pxs_balance(), PXS_asset( 5000 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pxs_savings(), PXS_asset( 3000 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pxs_rewards(), PXS_asset( 2000 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_vesting(), vested_7 );
      BOOST_REQUIRE_EQUAL( old_treasury.get_vest_rewards(), vested_3 );
    }

    PXS_asset initial_treasury_balance = db->get_treasury().get_pxs_balance();
    generate_block();
    database_fixture::validate_database();

    {
      auto& old_treasury = db->get_account( OBSOLETE_TREASURY_ACCOUNT );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pixa_balance(), PIXA_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pixa_savings(), PIXA_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pixa_rewards(), PIXA_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pxs_balance(), PXS_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pxs_savings(), PXS_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_pxs_rewards(), PXS_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_vesting(), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( old_treasury.get_vest_rewards(), VEST_asset( 0 ) );
    }
    {
      auto& treasury = db->get_account( NEW_HIVE_TREASURY_ACCOUNT );
      BOOST_REQUIRE_EQUAL( treasury.get_pixa_balance(), PIXA_asset( 14999 ) ); //rounding during vest->hive conversion
      BOOST_REQUIRE_EQUAL( treasury.get_pixa_savings(), PIXA_asset( 3000 ) );
      BOOST_REQUIRE_EQUAL( treasury.get_pixa_rewards(), PIXA_asset( 2000 ) );
      BOOST_REQUIRE_EQUAL( treasury.get_pxs_balance(), PXS_asset( 5000 ) + initial_treasury_balance );
      BOOST_REQUIRE_EQUAL( treasury.get_pxs_savings(), PXS_asset( 3000 ) );
      BOOST_REQUIRE_EQUAL( treasury.get_pxs_rewards(), PXS_asset( 2000 ) );
      BOOST_REQUIRE_EQUAL( treasury.get_vesting(), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( treasury.get_vest_rewards(), VEST_asset( 0 ) );
    }
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( treasury_debt_ratio )
{
  try
  {
    ACTORS( DEFAULT_VESTING, (alice) );
    BOOST_TEST_MESSAGE( "After HF24 funds in the treasury don't count towards the PXS debt ratio" );
    set_price_feed( PXS_price( 1000, 10000 ) );
    generate_block();
    auto& dgpo = db->get_dynamic_global_properties();
    const auto before_pxs_print_rate = dgpo.get_pxs_print_rate();

    ISSUE_FUNDS("alice", PXS_asset( 50'000'000'000 ));
    const auto during_pxs_print_rate = dgpo.get_pxs_print_rate();

    transfer( "alice", db->get_treasury_name(), asset( 50000000000, PXS_SYMBOL ), "", alice_private_key );
    generate_block();
    const auto after_pxs_print_rate = dgpo.get_pxs_print_rate();

    BOOST_REQUIRE_EQUAL( after_pxs_print_rate, before_pxs_print_rate );
    BOOST_REQUIRE_NE( after_pxs_print_rate, during_pxs_print_rate );
    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif

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

#define DELEGATED_VESTS( account ) db->get_account( account ).get_delegated_vesting()
#define RECEIVED_VESTS( account ) db->get_account( account ).get_received_vesting()

BOOST_FIXTURE_TEST_SUITE( hf23_tests, hf23_database_fixture )

BOOST_AUTO_TEST_CASE( restore_accounts_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Saving during HF23 and restoring balances during HF24" );

    ACTORS( DEFAULT_VESTING, (alice0)(alice1)(alice2)(alice3)(alice4)(dude) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    std::set< std::string > accounts{ "alice0", "alice1", "alice2", "alice3", "alice4" };

    struct tmp_data
    {
      std::string name;

      PIXA_asset  balance;
      PXS_asset   pxs_balance;
    };
    auto cmp = []( const tmp_data& a, const tmp_data& b )
    {
      return std::strcmp( a.name.c_str(), b.name.c_str() ) < 0;
    };
    std::set< tmp_data, decltype( cmp ) > old_balances( cmp );

    uint32_t cnt = 1;
    for( auto& account : accounts )
    {
      ISSUE_FUNDS( account, PIXA_asset( cnt * 1000 ) );
      ISSUE_FUNDS( account, PXS_asset( cnt * 1000 + 1 ) );
      ++cnt;

      old_balances.emplace( tmp_data{ account, get_pixa_balance( account ), get_pxs_balance( account ) } );
    }
    {
      ISSUE_FUNDS( "dude", PIXA_asset( 68 ) );
      ISSUE_FUNDS( "dude", PXS_asset( 78 ) );
      generate_block();
    }
    {
      db->clear_accounts( accounts );

      for( auto& account : accounts )
      {
        auto& _acc = db->get_account( account );

        idump(( _acc.get_pixa_balance() ));
        idump(( _acc.get_pxs_balance() ));
        BOOST_REQUIRE_EQUAL( _acc.get_pixa_balance(), PIXA_asset( 0 ) );
        BOOST_REQUIRE_EQUAL( _acc.get_pxs_balance(), PXS_asset( 0 ) );
      }
      BOOST_REQUIRE_EQUAL( get_pixa_balance( "dude" ), PIXA_asset( 68 ) );
      BOOST_REQUIRE_EQUAL( get_pxs_balance( "dude" ), PXS_asset( 78 ) );
    }
    {
      auto accounts_ex = accounts;
      accounts_ex.insert( "dude" );
      accounts_ex.insert( "devil" );

      db->restore_accounts( accounts_ex );

      auto itr_old_balances = old_balances.begin();
      for( auto& account : accounts )
      {
        auto& _acc = db->get_account( account );

        BOOST_REQUIRE_EQUAL( account, itr_old_balances->name );
        BOOST_REQUIRE_EQUAL( _acc.get_pixa_balance(), itr_old_balances->balance );
        BOOST_REQUIRE_EQUAL( _acc.get_pxs_balance(), itr_old_balances->pxs_balance );

        ++itr_old_balances;
      }
      BOOST_REQUIRE_EQUAL( get_pixa_balance( "dude" ), PIXA_asset( 68 ) );
      BOOST_REQUIRE_EQUAL( get_pxs_balance( "dude" ), PXS_asset( 78 ) );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( restore_accounts_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Saving during HF23 and restoring balances during HF24" );

    ACTORS( DEFAULT_VESTING, (alice)(bob)(carol) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    ISSUE_FUNDS( "alice", PIXA_asset( 1'000'000 ) );
    ISSUE_FUNDS( "alice", PXS_asset( 20'000 ) );
    ISSUE_FUNDS( "bob", PIXA_asset( 900'000 ) );
    ISSUE_FUNDS( "bob", PXS_asset( 10'000 ) );
    ISSUE_FUNDS( "carol", PIXA_asset( 3'600'000 ) );
    ISSUE_FUNDS( "carol", PXS_asset( 3'500'000 ) );

    const std::set< std::string > accounts{ "alice", "bob" };

    {
      auto& _alice   = db->get_account( "alice" );
      auto& _bob     = db->get_account( "bob" );

      auto alice_balance      = _alice.get_pixa_balance();
      auto alice_pxs_balance  = _alice.get_pxs_balance();
      auto bob_balance        = _bob.get_pixa_balance();
      auto bob_pxs_balance    = _bob.get_pxs_balance();

      {
        db->gather_balance( _alice.get_name(), _alice.get_pixa_balance(), _alice.get_pxs_balance() );
        temp_PIXA_balance pixa_transfer;
        db->adjust_balance( "alice", pixa_transfer, -_alice.get_pixa_balance() );
        db->adjust_balance( db->get_treasury_name(), pixa_transfer, pixa_transfer.as_asset() );
        temp_PXS_balance pxs_transfer;
        db->adjust_balance( "alice", pxs_transfer, -_alice.get_pxs_balance() );
        db->adjust_balance( db->get_treasury_name(), pxs_transfer, pxs_transfer.as_asset() );
      }
      {
        db->gather_balance( _bob.get_name(), _bob.get_pixa_balance(), _bob.get_pxs_balance() );
        temp_PIXA_balance pixa_transfer;
        db->adjust_balance( "bob", pixa_transfer, -_bob.get_pixa_balance() );
        db->adjust_balance( db->get_treasury_name(), pixa_transfer, pixa_transfer.as_asset() );
        temp_PXS_balance pxs_transfer;
        db->adjust_balance( "bob", pxs_transfer, -_bob.get_pxs_balance() );
        db->adjust_balance( db->get_treasury_name(), pxs_transfer, pxs_transfer.as_asset() );
      }

      PIXA_asset _2000( 2'000'000 );
      PXS_asset _10( 10'000 );
      PIXA_asset _800( 800'000 );
      PXS_asset _70( 70'000 );
      PIXA_asset _5000( 5'000'000 );
      PXS_asset _4000( 4'000'000 );

      {
        //Create another balances for `alice`
        temp_PIXA_balance pixa_transfer;
        db->adjust_balance( "carol", pixa_transfer, -_2000 );
        db->adjust_balance( "alice", pixa_transfer, _2000 );
        temp_PXS_balance pxs_transfer;
        db->adjust_balance( "carol", pxs_transfer, -_10 );
        db->adjust_balance( "alice", pxs_transfer, _10 );

        generate_block();

        BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), _2000 );
        BOOST_REQUIRE_EQUAL( get_pxs_balance( "alice" ), _10 );
      }
      {
        //Create another balances for `bob`
        temp_PIXA_balance pixa_transfer;
        db->adjust_balance( "carol", pixa_transfer, -_800 );
        db->adjust_balance( "bob", pixa_transfer, _800 );
        temp_PXS_balance pxs_transfer;
        db->adjust_balance( "carol", pxs_transfer, -_70 );
        db->adjust_balance( "bob", pxs_transfer, _70 );

        generate_block();

        BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), _800 );
        BOOST_REQUIRE_EQUAL( get_pxs_balance( "bob" ), _70 );
      }

      std::set< std::string > restored_accounts = { "bob", "alice", "carol" };

      auto treasury_balance_start      = db->get_treasury().get_pixa_balance();
      auto treasury_pxs_balance_start  = db->get_treasury().get_pxs_balance();

      {
        auto last_supply = db->get_dynamic_global_properties().get_current_pxs_supply();

        ISSUE_FUNDS( db->get_treasury_name(), _5000 );
        ISSUE_FUNDS( db->get_treasury_name(), _4000 );

        generate_block();

        auto interest = db->get_dynamic_global_properties().get_current_pxs_supply() - last_supply - _4000;

        BOOST_REQUIRE_EQUAL( db->get_treasury().get_pixa_balance(), _5000 + treasury_balance_start );
        BOOST_REQUIRE_EQUAL( db->get_treasury().get_pxs_balance(), _4000 + treasury_pxs_balance_start + interest );

        treasury_balance_start     += _5000;
        treasury_pxs_balance_start += _4000 + interest;
      }

      database_fixture::validate_database();

      db->restore_accounts( restored_accounts );

      auto& _alice2           = db->get_account( "alice" );
      auto& _bob2             = db->get_account( "bob" );
      const auto& _treasury2 = db->get_treasury();

      BOOST_REQUIRE_EQUAL( _alice2.get_pixa_balance(), _2000 + alice_balance );
      BOOST_REQUIRE_EQUAL( _alice2.get_pxs_balance(), _10 + alice_pxs_balance );
      BOOST_REQUIRE_EQUAL( _bob2.get_pixa_balance(), _800 + bob_balance );
      BOOST_REQUIRE_EQUAL( _bob2.get_pxs_balance(), _70 + bob_pxs_balance );

      BOOST_REQUIRE_EQUAL( _treasury2.get_pixa_balance(), treasury_balance_start - ( alice_balance + bob_balance ) );
      BOOST_REQUIRE_EQUAL( _treasury2.get_pxs_balance(), treasury_pxs_balance_start - ( alice_pxs_balance + bob_pxs_balance ) );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

hive::chain::hf23_item get_balances( const account_object& obj )
{
  return hive::chain::hf23_item{ obj.get_pixa_balance(), obj.get_pxs_balance() };
}

bool cmp_hf23_item( const hive::chain::hf23_item& a, const hive::chain::hf23_item& b )
{
  return a.pixa_balance == b.pixa_balance && a.pxs_balance == b.pxs_balance;
}

BOOST_AUTO_TEST_CASE( save_test_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Saving one account balances" );

    ACTORS( DEFAULT_VESTING, (alice) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    ISSUE_FUNDS( "alice", PIXA_asset( 1'000'000 ) );
    ISSUE_FUNDS( "alice", PXS_asset( 20'000 ) );

    const std::set< std::string > accounts{ "alice", "bob" };

    {
      db->gather_balance( "alice", get_pixa_balance( "alice" ), get_pxs_balance( "alice" ) );
      //there is also "steem" - genesis account that is affected by HF23
    }
    {
      BOOST_REQUIRE_EQUAL( db->get_hardfork_property_object().h23_balances.size(), 2u );

      auto alice_balances = get_balances( db->get_account( "alice" ) );

      BOOST_REQUIRE_EQUAL( db->get_hardfork_property_object().h23_balances.size(), 2u );
      BOOST_REQUIRE( cmp_hf23_item( db->get_hardfork_property_object().h23_balances.begin()->second, alice_balances ) );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( save_test_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Saving accounts balances" );

    ACTORS( DEFAULT_VESTING, (alice)(bob) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    ISSUE_FUNDS( "alice", PIXA_asset( 1'000'000 ) );
    ISSUE_FUNDS( "bob", PIXA_asset( 2'000'000 ) );

    ISSUE_FUNDS( "alice", PXS_asset( 20'000 ) );

    const std::set< std::string > accounts{ "alice", "bob" };

    {
      vest( "alice", "alice", PIXA_asset( 10'000 ), alice_private_key );
      vest( "bob", "bob", PIXA_asset( 10'000 ), bob_private_key );

      db->gather_balance( "alice", get_pixa_balance( "alice" ), get_pxs_balance( "alice" ) );
      db->gather_balance( "bob", get_pixa_balance( "bob" ), get_pxs_balance( "bob" ) );
      //there is also "steem" - genesis account that is affected by HF23
    }
    {
      auto alice_balances = get_balances( db->get_account( "alice" ) );
      auto bob_balances = get_balances( db->get_account( "bob" ) );

      BOOST_REQUIRE_EQUAL( db->get_hardfork_property_object().h23_balances.size(), 3u );

      auto iter = db->get_hardfork_property_object().h23_balances.begin();

      BOOST_REQUIRE( cmp_hf23_item( iter->second, alice_balances ) );
      ++iter;
      BOOST_REQUIRE( cmp_hf23_item( iter->second, bob_balances ) );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_06 )
{
  try
  {
    BOOST_TEST_MESSAGE( "VEST delegations - object of type `vesting_delegation_expiration_object` is generated" );

    ACTORS( DEFAULT_VESTING, (alice)(bob) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    set_account_creation_fee( PIXA_asset( 1 ) ); // turn off minimum limit on delegation size
    generate_block();

    PIXA_asset _10( 10'000 );
    PIXA_asset _20( 20'000 );
    VEST_asset _1v( 1'000000 );
    VEST_asset _2v( 2'000000 );
    VEST_asset _3v( 3'000000 );
    PIXA_asset _1000( 1'000'000 );

    ISSUE_FUNDS( "alice", _1000 );
    ISSUE_FUNDS( "bob", _1000 );

    {
      vest( "alice", "alice", _10, alice_private_key );
      vest( "bob", "bob", _20, bob_private_key );
    }
    {
      delegate_vest( "alice", "bob", _3v, alice_private_key );
      generate_block();

      const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
      BOOST_REQUIRE( idx.lower_bound( alice_id ) == idx.end() );
      BOOST_REQUIRE( idx.lower_bound( bob_id ) == idx.end() );
    }
    {
      delegate_vest( "alice", "bob", _3v, alice_private_key );

      const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
      BOOST_REQUIRE( idx.lower_bound( alice_id ) == idx.end() );
      BOOST_REQUIRE( idx.lower_bound( bob_id ) == idx.end() );
    }
    {
      delegate_vest( "alice", "bob", _2v, alice_private_key );

      const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
      BOOST_REQUIRE( idx.lower_bound( alice_id ) != idx.end() );
      BOOST_REQUIRE( idx.lower_bound( bob_id ) == idx.end() );
    }
    {
      delegate_vest( "alice", "bob", _1v, alice_private_key );

      const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
      BOOST_REQUIRE( idx.lower_bound( alice_id ) != idx.end() );
      BOOST_REQUIRE( idx.lower_bound( bob_id ) == idx.end() );
    }
    {
      const auto& _bob = db->get_account( "bob" );
      db->clear_account( _bob );

      const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
      BOOST_REQUIRE( idx.lower_bound( alice_id ) != idx.end() );
      BOOST_REQUIRE( idx.lower_bound( bob_id ) == idx.end() );
    }
    {
      const auto& _alice = db->get_account( "alice" );
      db->clear_account( _alice );

      const auto& idx = db->get_index< vesting_delegation_expiration_index, by_account_expiration >();
      BOOST_REQUIRE( idx.lower_bound( alice_id ) == idx.end() );
      BOOST_REQUIRE( idx.lower_bound( bob_id ) == idx.end() );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_05 )
{
  try
  {
    BOOST_TEST_MESSAGE( "VEST delegations - more complex scenarios" );

    ACTORS( DEFAULT_VESTING, (alice)(bob)(carol) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    set_account_creation_fee( PIXA_asset( 1 ) ); // turn off minimum delegation limit
    generate_block();

    PIXA_asset _10( 10'000 );
    PIXA_asset _20( 20'000 );
    VEST_asset _1v( 1.000000 );
    VEST_asset _2v( 2.000000 );
    VEST_asset _3v( 3.000000 );
    PIXA_asset _1000( 1000'000 );

    ISSUE_FUNDS( "alice", _1000 );
    ISSUE_FUNDS( "bob", _1000 );
    ISSUE_FUNDS( "carol", _1000 );

    {
      vest( "alice", "alice", _10, alice_private_key );
      vest( "bob", "bob", _20, bob_private_key );
      vest( "carol", "carol", _10, carol_private_key );
    }
    {
      delegate_vest( "alice", "bob", _1v, alice_private_key );
      delegate_vest( "alice", "bob", _2v, alice_private_key );

      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "alice" ), _2v );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "bob" ), _2v );
    }
    {
      delegate_vest( "alice", "carol", _1v, alice_private_key );

      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "alice" ), _3v );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "bob" ), _2v );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "carol" ), _1v );
    }
    {
      delegate_vest( "carol", "bob", _1v, carol_private_key );

      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "alice" ), _3v );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "carol" ), _1v );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "bob" ), _3v );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "carol" ), _1v );
    }
    {
      const auto& _carol = db->get_account( "carol" );
      db->clear_account( _carol );

      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "alice" ), _3v );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "carol" ), VEST_asset( 0 ) );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "bob" ), _2v );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "carol" ), _1v );

      auto hf23_vop = get_last_operations(1)[0].get< hardfork_hive_operation >();
      BOOST_REQUIRE_EQUAL( hf23_vop.other_affected_accounts.size(), 1 );
      BOOST_REQUIRE_EQUAL( hf23_vop.other_affected_accounts.back(), "bob" );
    }
    {
      const auto& _alice = db->get_account( "alice" );
      db->clear_account( _alice );

      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "carol" ), VEST_asset( 0 ) );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "carol" ), VEST_asset( 0 ) );

      auto hf23_vop = get_last_operations(1)[0].get< hardfork_hive_operation >();
      BOOST_REQUIRE_EQUAL( hf23_vop.other_affected_accounts.size(), 2 );
      BOOST_REQUIRE_EQUAL( hf23_vop.other_affected_accounts.front(), "bob" );
      BOOST_REQUIRE_EQUAL( hf23_vop.other_affected_accounts.back(), "carol" );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_04 )
{
  try
  {
    BOOST_TEST_MESSAGE( "VEST delegations" );

    ACTORS( DEFAULT_VESTING, (alice)(bob)(carol) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    set_account_creation_fee( PIXA_asset( 1 ) ); // turn off minimum delegation limit
    generate_block();

    PIXA_asset _10( 10'000 );
    PIXA_asset _20( 20'000 );
    VEST_asset _1v( 1'000000 );
    VEST_asset _2v( 2'000000 );
    PIXA_asset _1000( 1'000'000 );

    ISSUE_FUNDS( "alice", _1000 );
    ISSUE_FUNDS( "bob", _1000 );
    ISSUE_FUNDS( "carol", _1000 );

    {
      vest( "alice", "alice", _10, alice_private_key );
      vest( "alice", "alice", _20, alice_private_key );

      delegate_vest( "alice", "bob", _1v, alice_private_key );
      BOOST_REQUIRE_NE( DELEGATED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "carol" ), VEST_asset( 0 ) );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_NE( RECEIVED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "carol" ), VEST_asset( 0 ) );

      auto previous = DELEGATED_VESTS( "alice" );

      delegate_vest( "alice", "carol", _2v, alice_private_key );

      auto next = DELEGATED_VESTS( "alice" );
      BOOST_REQUIRE_GT( next, previous );

      BOOST_REQUIRE_NE( DELEGATED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "carol" ), VEST_asset( 0 ) );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_NE( RECEIVED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_NE( RECEIVED_VESTS( "carol" ), VEST_asset( 0 ) );
    }
    {
      const auto& _alice = db->get_account( "alice" );
      db->clear_account( _alice );

      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "carol" ), VEST_asset( 0 ) );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "carol" ), VEST_asset( 0 ) );

      const auto& _bob = db->get_account( "bob" );
      db->clear_account( _bob );

      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( DELEGATED_VESTS( "carol" ), VEST_asset( 0 ) );

      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( RECEIVED_VESTS( "carol" ), VEST_asset( 0 ) );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_03 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Vesting every account to anothers accounts" );

    ACTORS( DEFAULT_VESTING, (alice)(bob)(carol) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    PIXA_asset _1( 1'000 );
    PIXA_asset _2( 2'000 );
    PIXA_asset _3( 3'000 );
    PIXA_asset _1000( 1'000'000 );

    ISSUE_FUNDS( "alice", _1000 );
    ISSUE_FUNDS( "bob", _1000 );
    ISSUE_FUNDS( "carol", _1000 );

    BOOST_REQUIRE_EQUAL( get_vesting( "alice" ), get_vesting( "bob" ) );
    BOOST_REQUIRE_EQUAL( get_vesting( "bob" ), get_vesting( "carol" ) );

    {
      vest( "alice", "bob", _1, alice_private_key );
      vest( "bob", "carol", _2, bob_private_key );
      vest( "carol", "alice", _3, carol_private_key );
      BOOST_REQUIRE_GT( get_vesting( "alice" ), get_vesting( "carol" ) );
      BOOST_REQUIRE_GT( get_vesting( "carol" ), get_vesting( "bob" ) );
    }
    {
      auto vest_bob = get_vesting( "bob" );
      auto vest_carol = get_vesting( "carol" );

      const auto& _alice = db->get_account( "alice" );
      db->clear_account( _alice );

      BOOST_REQUIRE_EQUAL( get_vesting( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( get_vesting( "bob" ), vest_bob );
      BOOST_REQUIRE_EQUAL( get_vesting( "carol" ), vest_carol );

      const auto& _bob = db->get_account( "bob" );
      db->clear_account( _bob );

      BOOST_REQUIRE_EQUAL( get_vesting( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( get_vesting( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( get_vesting( "carol" ), vest_carol );

      const auto& _carol = db->get_account( "carol" );
      db->clear_account( _carol );

      BOOST_REQUIRE_EQUAL( get_vesting( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( get_vesting( "bob" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( get_vesting( "carol" ), VEST_asset( 0 ) );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_02 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Vesting" );

    ACTORS( DEFAULT_VESTING, (alice)(bob) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    PIXA_asset _1( 1'000 );
    PIXA_asset _1000( 1'000'000 );

    ISSUE_FUNDS( "alice", _1000 );
    ISSUE_FUNDS( "bob", _1000 );

    BOOST_REQUIRE_EQUAL( get_vesting( "alice" ), get_vesting( "bob" ) );

    {
      vest( "alice", "alice", _1, alice_private_key );
      BOOST_REQUIRE_GT( get_vesting( "alice" ), get_vesting( "bob" ) );
    }
    {
      auto vest_bob = get_vesting( "bob" );

      const auto& _alice = db->get_account( "alice" );
      db->clear_account( _alice );

      BOOST_REQUIRE_EQUAL( get_vesting( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( get_vesting( "bob" ), vest_bob );

      const auto& _bob = db->get_account( "bob" );
      db->clear_account( _bob );

      BOOST_REQUIRE_EQUAL( get_vesting( "alice" ), VEST_asset( 0 ) );
      BOOST_REQUIRE_EQUAL( get_vesting( "bob" ), VEST_asset( 0 ) );
    }

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( basic_test_01 )
{
  try
  {
    BOOST_TEST_MESSAGE( "Calling clear_account" );

    ACTORS( DEFAULT_VESTING, (alice) );
    generate_block();

    const auto& _alice = db->get_account( "alice" );

    db->clear_account( _alice );

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

#define CLEAR( who ) \
  if( !db->pending_transaction_session().has_value() ) \
    db->pending_transaction_session().emplace( db->start_undo_session(), db->start_undo_session() ); \
  db->clear_account( db->get_account( who ) ); \
  database_fixture::validate_database()
//since generating block runs undo and reapplies transactions since last block (and clear_account is not a transaction)
//it is better to do it explicitly to avoid confusion when reading and debugging test
#define UNDO_CLEAR db->pending_transaction_session().reset()

BOOST_AUTO_TEST_CASE( escrow_cleanup_test )
{

#define REQUIRE_BALANCE( alice, bob, carol, treasury, getter, asset ) \
  BOOST_REQUIRE_EQUAL( getter( "alice" ), asset( alice ) ); \
  BOOST_REQUIRE_EQUAL( getter( "bob" ), asset( bob ) ); \
  BOOST_REQUIRE_EQUAL( getter( "carol" ), asset( carol ) ); \
  BOOST_REQUIRE_EQUAL( getter( db->get_treasury_name() ), asset( treasury ) )

  try
  {
    BOOST_TEST_MESSAGE( "Calling escrow_cleanup_test" );

    ACTORS( DEFAULT_VESTING, (alice)(bob)(carol) );
    generate_block();

    issue_funds( "alice", PIXA_asset( 10'000 ) ); //<- note! extra 0.1 is in form of vests
    issue_funds( "alice", PXS_asset( 10'100 ) ); //<- note! treasury will get extras from interest and proposal-fund/inflation
    generate_block();
    REQUIRE_BALANCE( 10'000, 0, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 10'100, 0, 0, 1422, get_pxs_balance, PXS_asset );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    {
      escrow_transfer_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.pixa_amount = ASSET( "10.000 TESTS" );
      op.pxs_amount = ASSET( "10.000 TBD" );
      op.fee = ASSET( "0.100 TBD" );
      op.json_meta = "";
      op.ratification_deadline = db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL * 10 );
      op.escrow_expiration = db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL * 20 );
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 0, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 0, 0, 1422, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 1 );
    generate_block();

    //escrow transfer requested but neither receiver nor agent approved yet
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 0, 0, 10'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 0, 0, 11996, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;

    {
      escrow_approve_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "carol";
      tx.operations.push_back( op );
      push_transaction( tx, carol_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 0, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 0, 0, 1896, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 1 );
    generate_block();

    //escrow transfer approved by agent but not by receiver
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 0, 0, 10'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 0, 0, 12470, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;

    {
      escrow_approve_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "bob";
      tx.operations.push_back( op );
      push_transaction( tx, bob_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 0, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 0, 100, 2370, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 1 );
    generate_block();

    //escrow transfer approved by all parties (agent got fee) but the transfer itself wasn't released yet
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 0, 0, 10'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 0, 100, 12844, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;

    {
      escrow_release_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "alice";
      op.receiver = "bob";
      op.pixa_amount = ASSET( "2.000 TESTS" );
      op.pxs_amount = ASSET( "3.000 TBD" );
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 2'000, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 3'000, 100, 2844, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 1 );
    generate_block();

    //escrow transfer released partially by sender prior to escrow expiration
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 2'000, 0, 8'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 3'000, 100, 10318, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;

    {
      escrow_release_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "bob";
      op.receiver = "alice";
      op.pixa_amount = ASSET( "2.000 TESTS" );
      op.pxs_amount = ASSET( "3.000 TBD" );
      tx.operations.push_back( op );
      push_transaction( tx, bob_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 2'000, 2'000, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 3'000, 3'000, 100, 3318, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 1 );
    generate_block();

    //escrow transfer released partially by receiver prior to escrow expiration
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 2'000, 0, 8'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 3'000, 100, 10792, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;

    //after approvals it doesn't matter if the ratification expires

    {
      escrow_dispute_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "bob";
      tx.operations.push_back( op );
      push_transaction( tx, bob_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 2'000, 2'000, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 3'000, 3'000, 100, 3792, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 1 );
    generate_block();

    //escrow transfer disputed by sender
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 2'000, 0, 8'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 3'000, 100, 11266, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;

    //after dispute it doesn't matter if the escrow expires
    
    {
      escrow_release_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "carol";
      op.receiver = "bob";
      op.pixa_amount = ASSET( "2.000 TESTS" );
      op.pxs_amount = ASSET( "3.000 TBD" );
      tx.operations.push_back( op );

      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "carol";
      op.receiver = "alice";
      op.pixa_amount = ASSET( "2.000 TESTS" );
      op.pxs_amount = ASSET( "1.000 TBD" );
      tx.operations.push_back( op );

      push_transaction( tx, carol_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 4'000, 4'000, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 4'000, 6'000, 100, 4266, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) != nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 1 );
    generate_block();

    //escrow transfer released partially by agent to sender and partially to receiver
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 4'000, 0, 6'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 6'000, 100, 8740, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;

    {
      escrow_release_operation op;
      op.from = "alice";
      op.to = "bob";
      op.agent = "carol";
      op.who = "carol";
      op.receiver = "bob";
      op.pixa_amount = ASSET( "2.000 TESTS" );
      op.pxs_amount = ASSET( "0.000 TBD" );
      tx.operations.push_back( op );
      push_transaction( tx, carol_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 4'000, 6'000, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 4'000, 6'000, 100, 4740, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    generate_block();

    //escrow transfer released by agent to receiver and finished (transfer fully executed)
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 6'000, 0, 4'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 6'000, 100, 9214, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_escrow( "alice", 30 ) == nullptr );
    BOOST_REQUIRE_EQUAL( db->get_account( "alice" ).pending_escrow_transfers, 0 );
    UNDO_CLEAR;
  }
  FC_LOG_AND_RETHROW()

#undef REQUIRE_BALANCE

}

BOOST_AUTO_TEST_CASE( limit_order_cleanup_test )
{

#define REQUIRE_BALANCE( alice, bob, treasury, getter, asset ) \
  BOOST_REQUIRE_EQUAL( getter( "alice" ), asset( alice ) ); \
  BOOST_REQUIRE_EQUAL( getter( "bob" ), asset( bob ) ); \
  BOOST_REQUIRE_EQUAL( getter( db->get_treasury_name() ), asset( treasury ) )

  try
  {
    BOOST_TEST_MESSAGE( "Calling limit_order_cleanup_test" );
    ACTORS( DEFAULT_VESTING, (alice)(bob) );
    generate_block();

    issue_funds( "alice", PIXA_asset( 10'000 ) ); //<- note! extra 0.1 is in form of vests
    issue_funds( "bob", PXS_asset( 5'000 ) ); //<- note! treasury will get extras from interest and proposal-fund/inflation
    generate_block();
    REQUIRE_BALANCE( 10'000, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 5'000, 1422, get_pxs_balance, PXS_asset );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    {
      limit_order_create_operation op;
      op.owner = "alice";
      op.amount_to_sell = ASSET( "10.000 TESTS" );
      op.min_to_receive = ASSET( "5.000 TBD" );
      op.expiration = db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL * 20 );
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 5'000, 1422, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) != nullptr );
    generate_block();

    //limit order set but not matched yet
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 0, 10'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 5'000, 1896, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
    UNDO_CLEAR;

    {
      limit_order_create_operation op;
      op.owner = "bob";
      op.amount_to_sell = ASSET( "2.000 TBD" );
      op.min_to_receive = ASSET( "3.500 TESTS" );
      op.fill_or_kill = true;
      op.expiration = db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL * 20 );
      tx.operations.push_back( op );
      push_transaction( tx, bob_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 4'000, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 2'000, 3'000, 1896, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) != nullptr );
    generate_block();

    //limit order partially matched
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 4'000, 6'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 3'000, 4370, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
    UNDO_CLEAR;

    {
      limit_order_create2_operation op;
      op.owner = "bob";
      op.amount_to_sell = ASSET( "3.000 TBD" );
      op.exchange_rate = price( ASSET( "2.000 TBD" ), ASSET( "5.000 TESTS" ) );
      op.fill_or_kill = true;
      op.expiration = db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL * 20 );
      tx.operations.push_back( op );
      HIVE_REQUIRE_THROW( push_transaction( tx, bob_private_key ), fc::exception );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 4'000, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 2'000, 3'000, 2370, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) != nullptr );
    generate_block();

    //limit order partially matched after counter-order in fill_or_kill mode that failed (no change)
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 4'000, 6'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 3'000, 4844, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
    UNDO_CLEAR;

    {
      limit_order_create2_operation op;
      op.owner = "bob";
      op.amount_to_sell = ASSET( "3.000 TBD" );
      op.exchange_rate = price( ASSET( "2.000 TBD" ), ASSET( "4.000 TESTS" ) );
      op.fill_or_kill = true;
      op.expiration = db->head_block_time() + fc::seconds( HIVE_BLOCK_INTERVAL * 20 );
      tx.operations.push_back( op );
      push_transaction( tx, bob_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 10'000, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 5'000, 0, 2844, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
    generate_block();

    //limit order fully matched
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 10'000, 100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 0, 8318, get_pxs_balance, PXS_asset );
    BOOST_REQUIRE( db->find_limit_order( "alice", 0 ) == nullptr );
    UNDO_CLEAR;
  }
  FC_LOG_AND_RETHROW()

#undef REQUIRE_BALANCE

}

BOOST_AUTO_TEST_CASE( convert_request_cleanup_test )
{

#define REQUIRE_BALANCE( alice, treasury, getter, asset ) \
  BOOST_REQUIRE_EQUAL( getter( "alice" ), asset( alice ) ); \
  BOOST_REQUIRE_EQUAL( getter( db->get_treasury_name() ), asset( treasury ) )

  try
  {
    BOOST_TEST_MESSAGE( "Calling convert_request_cleanup_test" );
    ACTORS( DEFAULT_VESTING, (alice) );
    generate_block();

    //note! extra 0.1 TESTS is in form of vests
    issue_funds( "alice", PXS_asset( 5'000 ) ); //<- note! treasury will get extras from interest and proposal-fund/inflation
    generate_block();
    REQUIRE_BALANCE( 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 5'000, 1422, get_pxs_balance, PXS_asset );

    signed_transaction tx;
    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    {
      convert_operation op;
      op.owner = "alice";
      op.amount = ASSET( "5.000 TBD" );
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key );
    }
    tx.clear();

    REQUIRE_BALANCE( 0, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 1422, get_pxs_balance, PXS_asset );
    generate_block();

    //conversion request created but not executed yet
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 6896, get_pxs_balance, PXS_asset );
    UNDO_CLEAR;

    generate_blocks( db->head_block_time() + HIVE_CONVERSION_DELAY );

    REQUIRE_BALANCE( 5'000, 0, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 2844, get_pxs_balance, PXS_asset );
    generate_block();

    //conversion request executed
    CLEAR( "alice" );
    REQUIRE_BALANCE( 0, 5'100, get_pixa_balance, PIXA_asset );
    REQUIRE_BALANCE( 0, 3318, get_pxs_balance, PXS_asset );
    UNDO_CLEAR;

    tx.set_expiration( db->head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
    {
      collateralized_convert_operation op;
      op.owner = "alice";
      op.amount = ASSET( "1.000 TESTS" );
      tx.operations.push_back( op );
      push_transaction( tx, alice_private_key );
    }
    tx.clear();

    //collateralized conversion was not present when account clearing was executed, clear_account is not prepared for it
    //note that if you decide to handle it, you have to consider that PXS was produced from thin air and was already transfered,
    //while collateral that should be partially burned during actual conversion is still in global supply; in other words you
    //should actually finish conversion instead of passing collateral to treasury, because otherwise the temporary state of
    //having more PIXA than is proper, will become permanent
    HIVE_REQUIRE_THROW( db->clear_account( db->get_account( "alice" ) ), fc::exception );

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()

#undef REQUIRE_BALANCE

}

BOOST_AUTO_TEST_CASE( pxs_test_01 )
{
  try
  {
    ACTORS( DEFAULT_VESTING, (alice) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    BOOST_REQUIRE_EQUAL( get_pxs_balance( "alice" ), PXS_asset( 0 ) );
    db->clear_account( db->get_account( "alice" ) );
    BOOST_REQUIRE_EQUAL( get_pxs_balance( "alice" ), PXS_asset( 0 ) );
    database_fixture::validate_database();

    issue_funds( "alice", PXS_asset( 1'000'000 ) );
    auto alice_pxs = get_pxs_balance( "alice" );
    BOOST_REQUIRE_EQUAL( alice_pxs, PXS_asset( 1'000'000 ) );
    db->clear_account( db->get_account( "alice" ) );
    BOOST_REQUIRE_EQUAL( get_pxs_balance( "alice" ), PXS_asset( 0 ) );
    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( pxs_test_02 )
{
  try
  {
    ACTORS( DEFAULT_VESTING, (alice)(bob) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    BOOST_REQUIRE_EQUAL( get_pxs_balance( "alice" ), PXS_asset( 0 ) );
    issue_funds( "alice", PXS_asset( 1'000'000 ) );
    auto alice_pxs = get_pxs_balance( "alice" );
    BOOST_TEST_MESSAGE( "treasury_pxs = " << asset_to_string( db->get_treasury().get_pxs_balance() ) );
    BOOST_TEST_MESSAGE( "alice_pxs = " << asset_to_string( alice_pxs ) );
    BOOST_REQUIRE_EQUAL( alice_pxs, PXS_asset( 1'000'000 ) );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_PXS_INTEREST_COMPOUND_INTERVAL_SEC ), true );

    transfer( "alice", "bob", ASSET( "1.000 TBD" ), "", alice_private_key );

    auto& gpo = db->get_dynamic_global_properties();

    BOOST_REQUIRE_EQUAL( gpo.get_pxs_interest_rate(), 0 );
    BOOST_REQUIRE_EQUAL( get_pxs_balance( "alice" ), alice_pxs - PXS_asset( 1'000 ) );

    database_fixture::validate_database();

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_PXS_INTEREST_COMPOUND_INTERVAL_SEC ), true );

    alice_pxs = get_pxs_balance( "alice" );
    BOOST_TEST_MESSAGE( "alice_pxs = " << asset_to_string( alice_pxs ) );
    BOOST_TEST_MESSAGE( "bob_pxs = " << asset_to_string( get_pxs_balance( "bob" ) ) );

    db->clear_account( db->get_account( "alice" ) );
    BOOST_REQUIRE_EQUAL( get_pxs_balance( "alice" ), PXS_asset( 0 ) );

    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( savings_test_01 )
{
  try
  {
    ACTORS( DEFAULT_VESTING, (alice) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    issue_funds( "alice", PXS_asset( 1'000'000 ) );

    transfer_to_savings( "alice", "alice", ASSET( "1000.000 TBD" ), "", alice_private_key );

    BOOST_REQUIRE_EQUAL( get_pxs_savings( "alice" ), PXS_asset( 1'000'000 ) );
    db->clear_account( db->get_account( "alice" ) );
    BOOST_REQUIRE_EQUAL( get_pxs_savings( "alice" ), PXS_asset( 0 ) );
    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE( savings_test_02 )
{
  try
  {
    ACTORS( DEFAULT_VESTING, (alice)(bob) );
    generate_block();

    set_price_feed( PXS_price( 1000, 1000 ) );
    generate_block();

    issue_funds( "alice", PXS_asset( 1'000'000 ) );

    transfer_to_savings( "alice", "alice", ASSET( "1000.000 TBD" ), "", alice_private_key );

    BOOST_REQUIRE_EQUAL( get_pxs_savings( "alice" ), PXS_asset( 1'000'000 ) );

    generate_blocks( db->head_block_time() + fc::seconds( HIVE_PXS_INTEREST_COMPOUND_INTERVAL_SEC ), true );

    BOOST_TEST_MESSAGE( "alice_savings_pxs before clear = " << asset_to_string( get_pxs_savings( "alice" ) ) );
    db->clear_account( db->get_account( "alice" ) );
    BOOST_TEST_MESSAGE( "alice_savings_pxs after clear = " << asset_to_string( get_pxs_savings( "alice" ) ) );
    BOOST_REQUIRE_EQUAL( get_pxs_savings( "alice" ), PXS_asset( 0 ) );
    database_fixture::validate_database();
  }
  FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_SUITE_END()

#endif

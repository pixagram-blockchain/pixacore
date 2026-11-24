#if defined IS_TEST_NET
#include <boost/test/unit_test.hpp>

#include "../db_fixture/clean_database_fixture.hpp"

#include <hive/chain/detail/state/global_property_object_multiindex.hpp>

using namespace hive::chain;
using namespace hive::protocol;
using namespace hive::plugins;

full_transaction_ptr make_transfer( const account_name_type& from, const account_name_type& to,
  const asset& tokens, const fc::ecc::private_key& key, database& db )
{
  transfer_operation op;
  op.from = from;
  op.to = to;
  op.amount = tokens;
  signed_transaction tx;
  tx.set_expiration( db.head_block_time() + HIVE_MAX_TIME_UNTIL_EXPIRATION );
  tx.operations.push_back( op );
  auto pack_type = serialization_mode_controller::get_current_pack();
  full_transaction_ptr ftx = full_transaction_type::create_from_signed_transaction( tx, pack_type, false );
  ftx->sign_transaction( std::vector<fc::ecc::private_key>{ key }, db.get_chain_id(), pack_type );
  return ftx;
};

void direct_transfer( const account_name_type& from, const account_name_type& to,
  const PIXA_asset& delta_from, const PIXA_asset& delta_to, database& db )
{
  ilog( "Transfering directly from ${from} to ${to}", ( from ) ( to ) );
  temp_PIXA_balance transfer;
  db.adjust_balance( from, transfer, delta_from );
  db.adjust_balance( to, transfer, delta_to );
}

BOOST_FIXTURE_TEST_SUITE( debug_node, clean_database_fixture )

BOOST_AUTO_TEST_CASE( vests_pixa_evaluation )
{
  auto _calculate_modifiers = [ this ]( const VEST_price& _new_price,
    const PIXA_asset& _total_vested_pixa, const VEST_asset& _total_vests,
    share_type pixa_modifier, share_type vest_modifier )
  {
    PIXA_asset _pixa_modifier;
    VEST_asset _vest_modifier;

    db_plugin->calculate_modifiers_according_to_new_price( _new_price, _total_vested_pixa, _total_vests, _pixa_modifier, _vest_modifier );

    char _buffer[500];
    sprintf( _buffer, "_pixa_modifier: ( %ld == %ld ) _vest_modifier: ( %ld == %ld )", _pixa_modifier.amount.value, pixa_modifier.value, _vest_modifier.amount.value, vest_modifier.value );
    BOOST_TEST_MESSAGE( _buffer );
    BOOST_CHECK_EQUAL( _pixa_modifier, PIXA_asset( pixa_modifier ) );
    BOOST_CHECK_EQUAL( _vest_modifier, VEST_asset( vest_modifier ) );
  };

  _calculate_modifiers( VEST_price( 500, 1 ), PIXA_asset( 13'044 ), VEST_asset( 12'863'762'116631 ), 25'727'511'189, 0 );

  _calculate_modifiers( VEST_price( 1, 1 ), PIXA_asset( 1'000 ), VEST_asset( 1000 ), 0, 0 );

  _calculate_modifiers( VEST_price( 2, 1 ), PIXA_asset( 1'000 ), VEST_asset( 1000 ), 0, 1'000 );
  _calculate_modifiers( VEST_price( 1, 2 ), PIXA_asset( 1'000 ), VEST_asset( 1000 ), 1'000, 0 );

  _calculate_modifiers( VEST_price( 1'000, 1 ), PIXA_asset( 1'000 ), VEST_asset( 500000 ), 0, 500'000 );
  _calculate_modifiers( VEST_price( 1, 1'000 ), PIXA_asset( 500'000 ), VEST_asset( 1000 ), 500'000, 0 );

  _calculate_modifiers( VEST_price( 400, 2 ), PIXA_asset( 1'000 ), VEST_asset( 1'000000 ), 4'000, 0 );

  _calculate_modifiers( VEST_price( 500, 1 ), PIXA_asset( 800'000 ), VEST_asset( 100'000000 ), 0, 300'000'000 );

  _calculate_modifiers( VEST_price( 3, 1 ), PIXA_asset( 1'000 ), VEST_asset( 5000 ), 666, 0 );
}

BOOST_AUTO_TEST_CASE( state_modification )
{
  auto _check_state = [ this ]( const VEST_price& _new_price )
  {
    generate_block();
    validate_database();

    auto& _dgpo = db->get_dynamic_global_properties();

    char _buffer[500];

    sprintf( _buffer, "dgpo.total_vesting_fund_pixa (%ld) dgpo.total_vesting_shares (%ld)", _dgpo.get_total_vesting_fund_pixa().amount.value, _dgpo.get_total_vesting_shares().amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    PIXA_asset _old_current_supply = _dgpo.get_current_supply();
    PIXA_asset _old_virtual_supply = _dgpo.get_virtual_supply();

    PIXA_asset _old_total_vested_pixa  = _dgpo.get_total_vesting_fund_pixa();
    VEST_asset _old_total_vests = _dgpo.get_total_vesting_shares();
    PIXA_asset _pixa_modifier;
    VEST_asset _vest_modifier;

    db_plugin->calculate_modifiers_according_to_new_price( _new_price, _old_total_vested_pixa, _old_total_vests, _pixa_modifier, _vest_modifier );

    db_plugin->debug_set_vest_price( _new_price );

    sprintf( _buffer, "dgpo.current_supply (%ld) == _old_current_supply (%ld) + _pixa_modifier (%ld)", _dgpo.get_current_supply().amount.value, _old_current_supply.amount.value, _pixa_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.virtual_supply (%ld) == _old_virtual_supply (%ld) + _pixa_modifier (%ld)", _dgpo.get_virtual_supply().amount.value, _old_virtual_supply.amount.value, _pixa_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_fund_pixa (%ld) == _old_total_vested_pixa (%ld) + _pixa_modifier (%ld)", _dgpo.get_total_vesting_fund_pixa().amount.value, _old_total_vested_pixa.amount.value, _pixa_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    sprintf( _buffer, "dgpo.total_vesting_shares (%ld) == _old_total_vests (%ld) + _vest_modifier (%ld)", _dgpo.get_total_vesting_shares().amount.value, _old_total_vests.amount.value, _vest_modifier.amount.value );
    BOOST_TEST_MESSAGE( _buffer );

    BOOST_CHECK_EQUAL( _dgpo.get_current_supply(), _old_current_supply + _pixa_modifier );
    BOOST_CHECK_EQUAL( _dgpo.get_virtual_supply(), _old_virtual_supply + _pixa_modifier );

    BOOST_CHECK_EQUAL( _dgpo.get_total_vesting_fund_pixa(), _old_total_vested_pixa + _pixa_modifier );
    BOOST_CHECK_EQUAL( _dgpo.get_total_vesting_shares(), _old_total_vests + _vest_modifier );

    generate_block();
    validate_database();
  };

  {
    BOOST_TEST_MESSAGE( "pixa_modifier == 0 and vest_modifier > 0" );
    _check_state( VEST_price( 2'000'000, 1 ) );
  }

  {
    BOOST_TEST_MESSAGE( "pixa_modifier > 0 and vest_modifier == 0" );
    _check_state( VEST_price( 10'000, 1 ) );
  }

}

BOOST_AUTO_TEST_CASE( debug_update_use_bug )
{
  BOOST_TEST_MESSAGE( "Illustration for bug in debug_set_vest_price" );

  db_plugin->debug_update( []( database& db )
  {
    BOOST_TEST_MESSAGE( "First set some value in state so we can check if it was changed later" );
    db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
    {
      gpo.set_maximum_block_size( HIVE_MIN_BLOCK_SIZE_LIMIT );
    } );
  } );

  generate_block();

  db_plugin->debug_set_vest_price( VEST_price( 2000, 1 ) );

  // when your call to debug_update refers to local variables you must make sure they will remain
  // alive at least until registered action can no longer be called again
  uint32_t BAADF00D = 0xBAADF00D;
  uint32_t* stack_cleaner = ( uint32_t* )alloca( sizeof( BAADF00D ) * 100 );
  std::fill_n( stack_cleaner, 100, BAADF00D );

  BOOST_REQUIRE_EQUAL( db->get_dynamic_global_properties().get_maximum_block_size(), HIVE_MIN_BLOCK_SIZE_LIMIT );
  const uint32_t new_value = HIVE_MIN_BLOCK_SIZE_LIMIT * 4;

  // in order to show the bug it is important that this debug_update is called in the same block as
  // previous one hidden inside debug_set_vest_price()
  db_plugin->debug_update( [new_value]( database& db )
  {
    BOOST_TEST_MESSAGE( "Second call to debug_update within the same block reveals problem" );
    db.modify( db.get_dynamic_global_properties(), [&]( dynamic_global_property_object& gpo )
    {
      gpo.set_maximum_block_size( new_value );
    } );
  } );

  BOOST_TEST_MESSAGE( "Make sure the value we've set in second debug_update was actually changed" );
  BOOST_REQUIRE_EQUAL( db->get_dynamic_global_properties().get_maximum_block_size(), new_value );

  generate_block();
  BOOST_TEST_MESSAGE( "The value should still be set after generating block" );
    // if we generated more blocks the value would eventually be overwritten from witness values
  BOOST_REQUIRE_EQUAL( db->get_dynamic_global_properties().get_maximum_block_size(), new_value );

  validate_database();
}

BOOST_AUTO_TEST_CASE( debug_update_with_explicit_hook )
{
  BOOST_TEST_MESSAGE( "Hooking debug_update to transactions" );

  ACTORS( DEFAULT_VESTING, (alice)(bob)(carol)(dan) );
  generate_block();

  auto alice_to_bob = make_transfer( "alice", "bob", ASSET( "1.000 TESTS" ), alice_private_key, *db );
  auto alice_to_carol = make_transfer( "alice", "carol", ASSET( "2.000 TESTS" ), alice_private_key, *db );
  auto bob_to_dan = make_transfer( "bob", "dan", ASSET( "0.700 TESTS" ), bob_private_key, *db );
  auto carol_to_dan = make_transfer( "carol", "dan", ASSET( "6.000 TESTS" ), carol_private_key, *db );

  // everyone starts with zero balance
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 0 ) );

  // make bindings of callbacks to transactions out of order (to show they are not executed immediately)

  db_plugin->debug_update( [&]( database& db ) // if it executed now, bob would go negative
  {
    PIXA_asset delta( 10 );
    direct_transfer( "bob", "alice", -delta, delta, db );
  }, 0, bob_to_dan->get_transaction_id() );

  db_plugin->debug_update( [&]( database& db )
  {
    PIXA_asset delta( 200 );
    direct_transfer( "initminer", "carol", -delta, delta, db );
  }, 0, alice_to_carol->get_transaction_id() );

  db_plugin->debug_update( [&]( database& db ) // if it ever executed, validate_database would fail
  {
    direct_transfer( "dan", "alice", -PIXA_asset( 100 ), PIXA_asset( 10'000 ), db );
  }, 0, carol_to_dan->get_transaction_id() );

  db_plugin->debug_update( [&]( database& db )
  {
    PIXA_asset delta( 3'100 );
    direct_transfer( "initminer", "alice", -delta, delta, db );
  }, 0, alice_to_bob->get_transaction_id() );

  // none of above debug_updates was executed yet
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 0 ) );

  auto& cp = get_chain_plugin();

  cp.push_transaction( alice_to_bob );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 3100 - 1000 ) ); // direct from initminer / normal to bob
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 1000 ) ); // normal from alice
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 0 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 0 ) );

  cp.push_transaction( alice_to_carol );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 3100 - 1000 - 2000 ) ); // ... / normal to carol
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 1000 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 200 + 2000 ) ); // direct from initminer / normal from alice
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 0 ) );

  generate_block();

  // direct transfers are properly reapplied when block is generated
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 3100 - 1000 - 2000 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 1000 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 200 + 2000 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 0 ) );

  // since bob was already given tokens by alice, he can transfer now
  cp.push_transaction( bob_to_dan );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 3100 - 1000 - 2000 + 10 ) ); // ... / direct from bob
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 1000 - 10 - 700 ) ); // ... / direct to alice / normal to dan
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 200 + 2000 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 700 ) ); // normal from bob

  // carol has not enough balance to make transfer - hooked callback will also have no lasting effect
  BOOST_REQUIRE_THROW( cp.push_transaction( carol_to_dan ), fc::assert_exception );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 3100 - 1000 - 2000 + 10 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 1000 - 10 - 700 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 200 + 2000 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 700 ) );

  generate_block();

  // once again, check balances after block generation
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 3100 - 1000 - 2000 + 10 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 1000 - 10 - 700 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 200 + 2000 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 700 ) );

  validate_database();
}

BOOST_AUTO_TEST_CASE( debug_update_transaction_order )
{
  BOOST_TEST_MESSAGE( "Calling many debug_update in the same block mixed with transactions" );

  ACTORS( DEFAULT_VESTING, (alice)(bob)(carol)(dan)(eric)(frank)(greg) );
  generate_block();

  // we are going to pass single token and leave increasing amount of satoshis in each step:
  // - initminer to alice (normal)
  // - alice to bob (direct on internal(1))
  // - bob to carol (direct hooked to next normal)
  // - carol to dan (normal)
  // - dan to eric (direct on internal(2))
  // - eric to frank (direct on internal(2))
  // - frank to greg (normal)
  // - greg to temp (direct on internal(3))
  // if the order is different than above one of the transfers will fail

  const PIXA_asset token( 1'000 );
  const PIXA_asset step( 1 );
  const PIXA_asset zero( 0 );

  auto carol_to_dan = make_transfer( "carol", "dan", ( token - step * 6 ).to_asset(), carol_private_key, *db );
  db_plugin->debug_update( [&]( database& db )
  {
    PIXA_asset delta = token - step * 3;
    direct_transfer( "bob", "carol", -delta, delta, db );
  }, 0, carol_to_dan->get_transaction_id() );

  // before any action
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), zero );

  // initminer -> alice
  fund( "alice", token );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), token );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), zero );

  // alice -> bob
  db_plugin->debug_update( [&]( database& db )
  {
    PIXA_asset delta = token - step * 1;
    direct_transfer( "alice", "bob", -delta, delta, db );
  } );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), ( step * 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), ( token - step * 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), zero );

  // bob -> carol -> dan
  get_chain_plugin().push_transaction( carol_to_dan );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), ( step * 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), ( step * 2 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), ( step * 3 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), ( token - step * 6 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), zero );

  // dan -> eric
  db_plugin->debug_update( [&]( database& db )
  {
    PIXA_asset delta = token - step * 10;
    direct_transfer( "dan", "eric", -delta, delta, db );
  } );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), ( step * 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), ( step * 2 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), ( step * 3 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), ( step * 4 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), ( token - step * 10 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), zero );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), zero );

  // eric -> frank
  db_plugin->debug_update( [&]( database& db )
  {
    PIXA_asset delta = token - step * 15;
    direct_transfer( "eric", "frank", -delta, delta, db );
  } );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), ( step * 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), ( step * 2 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), ( step * 3 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), ( step * 4 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), ( step * 5 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), ( token - step * 15 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), zero );

  // frank -> greg
  transfer( "frank", "greg", ( token - step * 21 ).to_asset(), "almost done", frank_private_key);
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), ( step * 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), ( step * 2 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), ( step * 3 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), ( step * 4 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), ( step * 5 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), ( step * 6 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), ( token - step * 21 ) );

  // greg -> temp
  db_plugin->debug_update( [&]( database& db )
  {
    PIXA_asset delta = token - step * 28;
    direct_transfer( "greg", HIVE_TEMP_ACCOUNT, -delta, delta, db );
  } );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), ( step * 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), ( step * 2 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), ( step * 3 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), ( step * 4 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), ( step * 5 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), ( step * 6 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), ( step * 7 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( HIVE_TEMP_ACCOUNT ), ( token - step * 28 ) );

  generate_block();

  // state of balances should be the same after generating block
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "alice" ), PIXA_asset( 1 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "bob" ), PIXA_asset( 2 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "carol" ), PIXA_asset( 3 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "dan" ), PIXA_asset( 4 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "eric" ), PIXA_asset( 5 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "frank" ), PIXA_asset( 6 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( "greg" ), PIXA_asset( 7 ) );
  BOOST_REQUIRE_EQUAL( get_pixa_balance( HIVE_TEMP_ACCOUNT ), PIXA_asset( 972 ) );

  validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
#endif

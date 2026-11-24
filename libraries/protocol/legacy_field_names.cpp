#include <hive/protocol/legacy_field_names.hpp>

#include <fc/exception/exception.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <map>
#include <string>

namespace hive { namespace protocol {

namespace {

// Hive's name -> Pixa's name, for every operation field (and nested structure) the rename touched
const std::map< std::string, std::string >& renamed_fields()
{
  static const std::map< std::string, std::string > names = {
    { "percent_hbd",               "percent_pxs" },               // comment_options
    { "reward_hive",               "reward_pixa" },               // claim_reward_balance
    { "reward_hbd",                "reward_pxs" },                // claim_reward_balance
    { "hbd_amount",                "pxs_amount" },                // escrow_transfer, escrow_release, escrow_rejected
    { "hive_amount",               "pixa_amount" },               // escrow_transfer, escrow_release, escrow_rejected
    { "hbd_interest_rate",         "pxs_interest_rate" },         // legacy_chain_properties (witness_update, pow)
    { "hbd_payout",                "pxs_payout" },                // author_reward, comment_benefactor_reward
    { "hive_payout",               "pixa_payout" },               // author_reward, comment_benefactor_reward
    { "hbd_out",                   "pxs_out" },                   // collateralized_convert_immediate_conversion
    { "hive_vested",               "pixa_vested" },               // transfer_to_vesting_completed
    { "hive_amount_in",            "pixa_amount_in" },            // dhf_conversion
    { "hbd_amount_out",            "pxs_amount_out" },            // dhf_conversion
    { "hbd_transferred",           "pxs_transferred" },           // hardfork_hive, hardfork_hive_restore
    { "hive_transferred",          "pixa_transferred" },          // hardfork_hive, hardfork_hive_restore
    { "total_hive_from_vests",     "total_pixa_from_vests" },     // hardfork_hive
    { "is_saved_into_hbd_balance", "is_saved_into_pxs_balance" }  // interest
  };
  return names;
}

bool rename_value( const fc::variant& in, fc::variant& out );

bool rename_object( const fc::variant_object& in, fc::variant& out )
{
  const auto& names = renamed_fields();
  bool changed = false;
  fc::mutable_variant_object result;
  for( const auto& entry : in )
  {
    fc::variant renamed_value;
    const bool value_changed = rename_value( entry.value(), renamed_value );
    const fc::variant& value = value_changed ? renamed_value : entry.value();
    changed = changed || value_changed;

    auto itr = names.find( entry.key() );
    if( itr == names.end() )
    {
      result( entry.key(), value );
      continue;
    }

    changed = true;
    if( in.contains( itr->second.c_str() ) )
    {
      // both names given: the entry under the new name is written on its own turn
      FC_ASSERT( fc::json::to_string( in[ itr->second ] ) == fc::json::to_string( entry.value() ),
        "${old_name} and ${new_name} are the same field and hold different values",
        ( "old_name", entry.key() )( "new_name", itr->second ) );
      continue;
    }
    result( itr->second, value );
  }
  if( changed )
    out = fc::variant( std::move( result ) );
  return changed;
}

bool rename_value( const fc::variant& in, fc::variant& out )
{
  if( in.is_object() )
    return rename_object( in.get_object(), out );
  if( !in.is_array() )
    return false;

  const fc::variants& items = in.get_array();
  fc::variants result;
  bool changed = false;
  for( size_t i = 0; i < items.size(); ++i )
  {
    fc::variant item;
    if( rename_value( items[ i ], item ) )
    {
      if( !changed )
      {
        result.reserve( items.size() );
        result.insert( result.end(), items.begin(), items.begin() + i );
        changed = true;
      }
      result.push_back( std::move( item ) );
    }
    else if( changed )
    {
      result.push_back( items[ i ] );
    }
  }
  if( changed )
    out = fc::variant( std::move( result ) );
  return changed;
}

} // anonymous namespace

bool rename_legacy_field_names( const fc::variant& in, fc::variant& out )
{
  return rename_value( in, out );
}

} } // hive::protocol

#pragma once

#include <fc/variant.hpp>

namespace hive { namespace protocol {

/**
 * Operation fields that hold PXS or PIXA amounts are named after Pixa (percent_pxs, reward_pixa,
 * pxs_amount, ...). Up to hardfork 32 the node named them as Hive does (percent_hbd, reward_hive,
 * hbd_amount, ...), so JSON written for older nodes is still read.
 *
 * When `in` (an operation in either JSON form) holds one of the old names, `out` receives a copy that
 * holds the new name in its place and the function returns true; otherwise it returns false and `out`
 * is untouched. An old and a new name for the same field are accepted together only when they hold the
 * same value: only one of them could match what was signed.
 *
 * Only object keys are renamed. The witness_set_properties keys ("hbd_interest_rate",
 * "hbd_exchange_rate") are not field names but data inside the signed operation, and stay as they are.
 */
bool rename_legacy_field_names( const fc::variant& in, fc::variant& out );

} } // hive::protocol

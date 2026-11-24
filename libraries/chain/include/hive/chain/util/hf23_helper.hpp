#pragma once

#include <hive/protocol/asset.hpp>

#include<string>

namespace hive { namespace chain {

using hive::protocol::PIXA_asset;
using hive::protocol::PXS_asset;

struct hf23_item
{
  PIXA_asset  pixa_balance;
  PXS_asset   pxs_balance;
};

} } // namespace hive::chain

FC_REFLECT( hive::chain::hf23_item,
  (pixa_balance)(pxs_balance) )

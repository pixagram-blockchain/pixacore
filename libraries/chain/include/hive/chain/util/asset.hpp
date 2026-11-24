#pragma once

#include <hive/protocol/asset.hpp>

namespace hive { namespace chain { namespace util {

using hive::protocol::PXS_asset;
using hive::protocol::PIXA_asset;
using hive::protocol::PXS_price;

inline PXS_asset to_pxs( const PXS_price& p, const PIXA_asset& pixa )
{
  if( p.is_null() )
    return PXS_asset( 0 );
  return pixa * p;
}

inline PIXA_asset to_pixa( const PXS_price& p, const PXS_asset& pxs )
{
  if( p.is_null() )
    return PIXA_asset( 0 );
  return pxs * p;
}

} } }

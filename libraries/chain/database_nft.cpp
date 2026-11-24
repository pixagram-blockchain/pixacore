#include <hive/chain/hive_fwd.hpp>

#include <hive/chain/database.hpp>
#include <hive/chain/index.hpp>
#include <hive/chain/detail/state/nft_object_multiindex.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>
#include <chainbase/chainbase.inl>

#include <hive/chain/util/type_registrar_definition.hpp>

/**
  * Native NFTs (HF32): the state indexes, the lookups and the policy of the feature.
  *
  * The rules of each action live in nft_evaluator.cpp. What those rules compare against - how many
  * copies, how high a royalty, which price assets, what mint fee - is answered by the four functions
  * at the end of this file and nowhere else. Lowering the copy cap or accepting PIXA as well, for
  * example, is a change to get_nft_max_editions() or is_nft_price_symbol() behind a new hardfork check.
  */

namespace hive { namespace chain {

void initialize_core_indexes_10( database& db )
{
  HIVE_ADD_CORE_INDEX( db, nft_index );
  HIVE_ADD_CORE_INDEX( db, nft_edition_index );
}

const nft_object* database::find_nft( const account_id_type& creator, const std::string& permlink )const
{
  const auto& idx = get_index< nft_index, by_nft_permlink >();
  auto itr = idx.find( boost::make_tuple( creator, permlink ) );
  // The index compares permlinks as C strings, so "art" and "art\0anything" find the same entry.
  // Requiring the same length makes the match exact: an operation can only name an NFT by the very
  // permlink it was minted with, and what indexers read in the operation is what the chain acted on.
  if( itr == idx.end() || itr->get_permlink().size() != permlink.size() )
    return nullptr;
  return &( *itr );
}

const nft_object* database::find_nft( const account_name_type& creator, const std::string& permlink )const
{
  const account_object* account = find_account( creator );
  return account == nullptr ? nullptr : find_nft( account->get_id(), permlink );
}

const nft_edition_object* database::find_nft_edition( const nft_object& nft, uint16_t edition )const
{
  const auto& idx = get_index< nft_edition_index, by_nft_edition >();
  auto itr = idx.find( boost::make_tuple( nft.get_id(), edition ) );
  return itr == idx.end() ? nullptr : &( *itr );
}

uint16_t database::find_nft_reserve_copy( const nft_object& nft )const
{
  if( nft.get_reserve() == 0 )
    return 0;
  // Every copy numbered below the start has an object. Walk the objects from there: the first number
  // without one is the copy. Each sale from the reserve moves the start past the copy it sold, so the
  // walk only crosses copies that were offered or given on their own since the previous sale.
  const auto& idx = get_index< nft_edition_index, by_nft_edition >();
  uint16_t copy = nft.get_reserve_start();
  for( auto itr = idx.lower_bound( boost::make_tuple( nft.get_id(), copy ) );
       itr != idx.end() && itr->get_nft() == nft.get_id() && itr->get_edition() == copy; ++itr )
    ++copy;
  return copy;
}

uint16_t database::get_nft_max_editions()const
{
  return PIXA_NFT_MAX_EDITIONS;
}

uint16_t database::get_nft_max_royalty()const
{
  return PIXA_NFT_MAX_ROYALTY;
}

bool database::is_nft_price_symbol( const asset_symbol_type& symbol )const
{
  // offers, and therefore sales and royalties, are in PXS; the operation format also takes PIXA
  return symbol == HBD_SYMBOL;
}

HBD_asset database::get_nft_mint_fee( uint16_t editions )const
{
  // 1.000 PXS per copy, so a mint pays for what it can put on the market. It does not depend on any
  // voted value: a signed mint is valid at the same price until it expires.
  return HBD_asset( int64_t( editions ) * PIXA_NFT_MINT_FEE_PER_EDITION );
}

} } // hive::chain

HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::nft_index )
HIVE_DEFINE_TYPE_REGISTRAR_REGISTER_TYPE( hive::chain::nft_edition_index )

// Explicit template instantiations for chainbase::database methods
template const chainbase::generic_index<hive::chain::nft_index>& chainbase::database::get_index<hive::chain::nft_index>() const;
template chainbase::generic_index<hive::chain::nft_index>& chainbase::database::get_mutable_index<hive::chain::nft_index>();

template const chainbase::generic_index<hive::chain::nft_edition_index>& chainbase::database::get_index<hive::chain::nft_edition_index>() const;
template chainbase::generic_index<hive::chain::nft_edition_index>& chainbase::database::get_mutable_index<hive::chain::nft_edition_index>();

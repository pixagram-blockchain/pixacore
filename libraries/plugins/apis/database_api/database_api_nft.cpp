#include <hive/plugins/database_api/database_api_impl.hpp>

#include <hive/chain/detail/state/nft_object_multiindex.hpp>
#include <hive/chain/detail/state/account_object_multiindex.hpp>

#include <algorithm>
#include <cctype>
#include <set>

namespace hive { namespace plugins { namespace database_api {

//////////////////////////////////////////////////////////////////////
//                                                                  //
// NFTs (HF32)                                                      //
//                                                                  //
//////////////////////////////////////////////////////////////////////

namespace
{
  /// What an NFT id names: copy `edition` of the NFT (author, permlink), or the NFT itself for 0.
  struct nft_id_parts
  {
    account_name_type author;
    std::string       permlink;
    uint16_t          edition = 0;
  };

  /**
    * Reads "@author/permlink/N". The author runs to the first '/' and N follows the last one, so a
    * permlink that holds a '/' (the chain accepts any UTF-8) still reads correctly. N is 0 for the
    * NFT and 1 to PIXA_NFT_EDITIONS_LIMIT for a copy, written without leading zeros.
    */
  nft_id_parts parse_nft_id( const std::string& id )
  {
    const auto first = id.find( '/' );
    const auto last = id.rfind( '/' );
    FC_ASSERT( id.size() > 1 && id[0] == '@' && first != std::string::npos && last > first + 1 && last + 1 < id.size(),
      "An NFT id is written @author/permlink/N, got '${id}'", ( "id", id ) );

    const std::string author = id.substr( 1, first - 1 );
    FC_ASSERT( protocol::is_valid_account_name( author ), "Invalid author in NFT id '${id}'", ( "id", id ) );

    const std::string number = id.substr( last + 1 );
    FC_ASSERT( number.size() <= 5 && std::all_of( number.begin(), number.end(), []( char c ) { return std::isdigit( (unsigned char)c ) != 0; } )
      && ( number == "0" || number[0] != '0' ), "Invalid copy number in NFT id '${id}'", ( "id", id ) );
    const uint32_t edition = std::stoul( number );
    FC_ASSERT( edition <= PIXA_NFT_EDITIONS_LIMIT, "Copy number above ${max} in NFT id '${id}'",
      ( "max", PIXA_NFT_EDITIONS_LIMIT )( "id", id ) );

    return nft_id_parts{ account_name_type( author ), id.substr( first + 1, last - first - 1 ), uint16_t( edition ) };
  }
}

DEFINE_API_IMPL( database_api_impl, get_nft_params )
{
  get_nft_params_return result;

  result.mint_fee_per_edition = _db.get_nft_mint_fee( 1 ).to_asset();
  result.max_editions = _db.get_nft_max_editions();
  result.max_royalty = _db.get_nft_max_royalty();
  result.max_license_length = PIXA_NFT_MAX_LICENSE_LENGTH;
  for( const asset_symbol_type& symbol : { HBD_SYMBOL, HIVE_SYMBOL } )
    if( _db.is_nft_price_symbol( symbol ) )
      result.price_symbols.push_back( symbol );

  return result;
}

/* An id that names nothing is simply absent from the answer, so the call doubles as the
   "is this post minted" check for one post or for a whole page of posts. Every NFT that an id
   names is returned once, and each copy asked for once. A copy that was never offered nor moved
   has no object; it is answered all the same, as its creator's since the mint. */
DEFINE_API_IMPL( database_api_impl, find_nfts )
{
  FC_ASSERT( args.nfts.size() <= DATABASE_API_NFT_FIND_LIMIT, "At most ${n} NFT ids per call", ( "n", DATABASE_API_NFT_FIND_LIMIT ) );

  find_nfts_return result;
  std::set< nft_id_type > nfts_added;
  std::set< std::pair< nft_id_type, uint16_t > > copies_added;

  for( const auto& id : args.nfts )
  {
    const nft_id_parts wanted = parse_nft_id( id );
    const chain::nft_object* nft = _db.find_nft( wanted.author, wanted.permlink );
    if( nft == nullptr || wanted.edition > nft->get_editions() )
      continue;

    if( nfts_added.insert( nft->get_id() ).second )
      result.nfts.emplace_back( *nft, _db );

    if( wanted.edition != 0 && copies_added.insert( std::make_pair( nft->get_id(), wanted.edition ) ).second )
    {
      const chain::nft_edition_object* copy = _db.find_nft_edition( *nft, wanted.edition );
      if( copy != nullptr )
        result.editions.emplace_back( *copy, _db );
      else
        result.editions.emplace_back( *nft, wanted.edition, _db );
    }
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, list_nfts )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_nfts_return result;
  result.nfts.reserve( args.limit );

  switch( args.order )
  {
    case( by_creator ):
    {
      // NFTs are indexed by the id of the creator; a name that is not an account has no NFTs after it
      std::optional< boost::tuple< account_id_type, std::string > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, std::string > >();
        const chain::account_object* creator = _db.find_account( key.first );
        FC_ASSERT( creator != nullptr, "Given creator account does not exist." );
        start = boost::make_tuple( creator->get_id(), key.second );
      }
      iterate_results< chain::nft_index, chain::by_nft_permlink >(
        start,
        result.nfts,
        args.limit,
        &database_api_impl::on_push_default< api_nft_object, nft_object >,
        &database_api_impl::filter_default< nft_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

DEFINE_API_IMPL( database_api_impl, list_nft_editions )
{
  FC_ASSERT( 0 < args.limit && args.limit <= DATABASE_API_SINGLE_QUERY_LIMIT, "limit not set or too big" );

  list_nft_editions_return result;
  result.editions.reserve( args.limit );

  switch( args.order )
  {
    case( by_permlink ):
    {
      // the copies of one NFT in number order; those without an object are their creator's
      FC_ASSERT( args.start.is_string(), "by_permlink starts at a copy: \"@author/permlink/N\"" );
      const nft_id_parts first = parse_nft_id( args.start.as_string() );
      const chain::nft_object* nft = _db.find_nft( first.author, first.permlink );
      FC_ASSERT( nft != nullptr, "Given NFT does not exist." );
      for( uint32_t edition = std::max< uint32_t >( first.edition, 1 );
           edition <= nft->get_editions() && result.editions.size() < args.limit; ++edition )
      {
        const chain::nft_edition_object* copy = _db.find_nft_edition( *nft, uint16_t( edition ) );
        if( copy != nullptr )
          result.editions.emplace_back( *copy, _db );
        else
          result.editions.emplace_back( *nft, uint16_t( edition ), _db );
      }
      break;
    }
    case( by_account ):
    {
      // copies that have an object, by owner, from the copy named in start (included, as in the
      // other list methods: pass the last copy of a page to get the next page)
      std::optional< boost::tuple< account_id_type, nft_edition_id_type > > start;
      if( !args.start.is_null() )
      {
        auto key = args.start.as< std::pair< account_name_type, std::string > >();
        const chain::account_object* owner = _db.find_account( key.first );
        FC_ASSERT( owner != nullptr, "Given owner account does not exist." );
        nft_edition_id_type from;
        if( !key.second.empty() )
        {
          const nft_id_parts copy_id = parse_nft_id( key.second );
          const chain::nft_object* nft = _db.find_nft( copy_id.author, copy_id.permlink );
          const chain::nft_edition_object* copy = nft == nullptr ? nullptr : _db.find_nft_edition( *nft, copy_id.edition );
          FC_ASSERT( copy != nullptr, "The start has to be a copy returned by a previous page." );
          from = copy->get_id();
        }
        start = boost::make_tuple( owner->get_id(), from );
      }
      iterate_results< chain::nft_edition_index, chain::by_nft_owner >(
        start,
        result.editions,
        args.limit,
        &database_api_impl::on_push_default< api_nft_edition_object, nft_edition_object >,
        &database_api_impl::filter_default< nft_edition_object > );
      break;
    }
    default:
      FC_ASSERT( false, "Unknown or unsupported sort order '${o}'", ( "o", args.order ) );
  }

  return result;
}

} } } // hive::plugins::database_api

#include <hive/protocol/nft_operations.hpp>

#include <hive/protocol/config.hpp>
#include <hive/protocol/validation.hpp>

/**
  * Stateless checks of nft_operation.
  *
  * Only the limits of the operation format are checked here, because validate() runs at every
  * hardfork. The limits consensus applies (number of editions, royalty ceiling, price asset, mint fee)
  * can change with a hardfork, so they are checked by the evaluator (libraries/chain/nft_evaluator.cpp).
  * That is why a price in PIXA passes this check: the format takes a liquid asset, and consensus takes
  * PXS only (database::is_nft_price_symbol()).
  */

namespace hive { namespace protocol {

  namespace
  {
    // one copy: 1 to the most copies an NFT can have
    void validate_nft_id( const account_name_type& author, const string& permlink, uint16_t edition )
    {
      validate_account_name( author );
      validate_permlink( permlink );
      HIVE_PROTOCOL_NUMBER_ASSERT_WITH_SUBJECT( edition >= 1 && edition <= PIXA_NFT_EDITIONS_LIMIT && "nft_edition", edition,
        "NFT edition number must be between 1 and ${max}. Value: ${subject}", ("max", PIXA_NFT_EDITIONS_LIMIT) );
    }

    // one copy, or 0 for the reserve of the creator (offered, withdrawn and bought as a whole)
    void validate_nft_id_or_reserve( const account_name_type& author, const string& permlink, uint16_t edition )
    {
      validate_account_name( author );
      validate_permlink( permlink );
      HIVE_PROTOCOL_NUMBER_ASSERT_WITH_SUBJECT( edition <= PIXA_NFT_EDITIONS_LIMIT && "nft_edition_or_reserve", edition,
        "NFT edition number must be between 0 (the reserve) and ${max}. Value: ${subject}", ("max", PIXA_NFT_EDITIONS_LIMIT) );
    }

    void validate_nft_price( const asset& price )
    {
      HIVE_PROTOCOL_ASSET_ASSERT_WITH_SUBJECT(
        ( is_asset_type( price, HBD_SYMBOL ) || is_asset_type( price, HIVE_SYMBOL ) ) && "nft_price", price,
        "NFT price must be expressed in a liquid asset. Actual: ${subject}" );
      validate_asset_greater_than_zero( price, "NFT price must be positive" );
    }

    struct nft_action_validate_visitor
    {
      typedef void result_type;

      template< typename Action >
      void operator()( const Action& action ) const { action.validate(); }
    };
  }

  void nft_mint::validate() const
  {
    validate_permlink( permlink );
    HIVE_PROTOCOL_NUMBER_ASSERT_WITH_SUBJECT( editions >= 1 && editions <= PIXA_NFT_EDITIONS_LIMIT && "nft_mint_editions", editions,
      "Number of NFT editions must be between 1 and ${max}. Value: ${subject}", ("max", PIXA_NFT_EDITIONS_LIMIT) );
    validate_number_in_100_percent_range( royalty, "NFT royalty" );
    validate_string_max_size( license, PIXA_NFT_MAX_LICENSE_LENGTH, "NFT license is too long" );
    validate_is_utf8( license, "NFT license is not UTF8" );
    validate_asset_type( fee, HBD_SYMBOL, "NFT mint fee must be PXS" );
    validate_asset_not_negative( fee, "NFT mint fee cannot be negative" );
  }

  void nft_list::validate() const
  {
    validate_nft_id_or_reserve( author, permlink, edition );
    validate_nft_price( price );
  }

  void nft_unlist::validate() const
  {
    validate_nft_id_or_reserve( author, permlink, edition );
  }

  void nft_buy::validate() const
  {
    validate_nft_id_or_reserve( author, permlink, edition );
    validate_nft_price( price );
  }

  void nft_transfer::validate() const
  {
    validate_nft_id( author, permlink, edition );
    validate_account_name( to );
  }

  void nft_operation::validate() const
  {
    validate_account_name( account );
    action.visit( nft_action_validate_visitor() );

    if( action.which() == nft_action::tag< nft_transfer >::value )
    {
      HIVE_PROTOCOL_ACCOUNT_NAME_ASSERT( action.get< nft_transfer >().to != account && "nft_transfer_to_self",
        "An NFT cannot be transferred to the account that sends it.", ("subject", account) );
    }
  }

} } // hive::protocol

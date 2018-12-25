#include "InventoryMgr.h"

#include <Common.h>
#include "Actor/Player.h"
#include "Inventory/ItemContainer.h"
#include "Inventory/Item.h"
#include "Inventory/ItemUtil.h"
#include <Network/PacketDef/Zone/ServerZoneDef.h>
#include <Network/GamePacketNew.h>

#include <Database/DatabaseDef.h>
#include <Exd/ExdDataGenerated.h>

#include "Framework.h"

extern Sapphire::Framework g_fw;

using namespace Sapphire::Network::Packets;

void Sapphire::World::Manager::InventoryMgr::sendInventoryContainer( Sapphire::Entity::Player& player,
                                                                     Sapphire::ItemContainerPtr container )
{
  auto sequence = player.getNextInventorySequence();
  auto pMap = container->getItemMap();

  for( auto itM = pMap.begin(); itM != pMap.end(); ++itM )
  {
    if( !itM->second )
      return;

    if( container->getId() == Common::InventoryType::Currency || container->getId() == Common::InventoryType::Crystal )
    {
      auto currencyInfoPacket = makeZonePacket< Server::FFXIVIpcCurrencyCrystalInfo >( player.getId() );
      currencyInfoPacket->data().containerSequence = sequence;
      currencyInfoPacket->data().catalogId = itM->second->getId();
      currencyInfoPacket->data().unknown = 1;
      currencyInfoPacket->data().quantity = itM->second->getStackSize();
      currencyInfoPacket->data().containerId = container->getId();
      currencyInfoPacket->data().slot = 0;

      player.queuePacket( currencyInfoPacket );
    }
    else
    {
      auto itemInfoPacket = makeZonePacket< Server::FFXIVIpcItemInfo >( player.getId() );
      itemInfoPacket->data().containerSequence = sequence;
      itemInfoPacket->data().containerId = container->getId();
      itemInfoPacket->data().slot = itM->first;
      itemInfoPacket->data().quantity = itM->second->getStackSize();
      itemInfoPacket->data().catalogId = itM->second->getId();
      itemInfoPacket->data().condition = itM->second->getDurability();
      itemInfoPacket->data().spiritBond = 0;
      itemInfoPacket->data().hqFlag = itM->second->isHq() ? 1 : 0;
      itemInfoPacket->data().stain = itM->second->getStain();

      player.queuePacket( itemInfoPacket );
    }
  }

  auto containerInfoPacket = makeZonePacket< Server::FFXIVIpcContainerInfo >( player.getId() );
  containerInfoPacket->data().containerSequence = sequence;
  containerInfoPacket->data().numItems = container->getEntryCount();
  containerInfoPacket->data().containerId = container->getId();

  player.queuePacket( containerInfoPacket );
}

Sapphire::ItemPtr Sapphire::World::Manager::InventoryMgr::createItem( Entity::Player& player,
                                                                      uint32_t catalogId, uint32_t quantity )
{
  auto pExdData = g_fw.get< Data::ExdDataGenerated >();
  auto pDb = g_fw.get< Db::DbWorkerPool< Db::ZoneDbConnection > >();
  auto itemInfo = pExdData->get< Sapphire::Data::Item >( catalogId );

  if( !itemInfo )
    return nullptr;

  auto item = make_Item( Items::Util::getNextUId(), catalogId,
                          itemInfo->modelMain, itemInfo->modelSub );

  item->setStackSize( std::max< uint32_t >( 1, quantity ) );

  auto stmt = pDb->getPreparedStatement( Db::CHARA_ITEMGLOBAL_INS );

  stmt->setUInt( 1, player.getId() );
  stmt->setUInt64( 2, item->getUId() );
  stmt->setUInt( 3, item->getId() );
  stmt->setUInt( 4, item->getStackSize() );

  pDb->directExecute( stmt );

  return item;
}

void Sapphire::World::Manager::InventoryMgr::saveHousingContainer( Common::LandIdent ident,
                                                                   Sapphire::ItemContainerPtr container )
{
  auto u64ident = *reinterpret_cast< uint64_t* >( &ident );

  for( auto& item : container->getItemMap() )
  {
    saveHousingContainerItem( u64ident, container->getId(), item.first, item.second->getUId() );
  }
}

void Sapphire::World::Manager::InventoryMgr::saveHousingContainerItem( uint64_t ident,
                                                                       uint16_t containerId, uint16_t slotId,
                                                                       uint64_t itemId )
{
  auto pDb = g_fw.get< Db::DbWorkerPool< Db::ZoneDbConnection > >();

  auto stmt = pDb->getPreparedStatement( Db::LAND_INV_UP );
  // LandIdent, ContainerId, SlotId, ItemId, ItemId

  stmt->setUInt64( 1, ident );
  stmt->setUInt( 2, containerId );
  stmt->setUInt( 3, slotId );
  stmt->setUInt64( 4, itemId );

  // see query, we have to pass itemid in twice
  // the second time is for the ON DUPLICATE KEY UPDATE condition
  stmt->setUInt64( 5, itemId );

  pDb->execute( stmt );
}
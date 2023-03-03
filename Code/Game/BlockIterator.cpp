#include "Game/BlockIterator.hpp"
#include "Game/Block.hpp"
#include "Game/Chunk.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/AABB3.hpp"

Block* BlockIterator::GetBlock() const
{
	if (m_chunkBlockBelongsTo == nullptr)
		return nullptr;
	return m_chunkBlockBelongsTo->GetBlock(m_blockIndex);
}

Vec3 BlockIterator::GetWorldCenter() const
{
	AABB3 worldChunkBounds = m_chunkBlockBelongsTo->GetChunkWorldBounds();
	IntVec3 localBlockCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	return Vec3(worldChunkBounds.m_mins + Vec3(localBlockCoords) + Vec3(0.5f, 0.5f, 0.5f));
}

AABB3 BlockIterator::GetBlockBounds() const
{
	AABB3 worldChunkBounds = m_chunkBlockBelongsTo->GetChunkWorldBounds();
	IntVec3 localBlockCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	Vec3 blockBoundsMins = worldChunkBounds.m_mins + Vec3(localBlockCoords);
	return AABB3(blockBoundsMins, blockBoundsMins + Vec3::ONE);
}

bool BlockIterator::IsBlockOpaque() const
{
	return BlockDefintion::IsBlockTypeOpaque(GetBlock()->m_typeIndex);
}

BlockIterator BlockIterator::GetEastNeighbour() const
{
	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.x == CHUNK_MAX_X)
	{
		blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo->m_eastNeighbour;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(0, localCoords.y, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo;
	blockIterator.m_blockIndex = m_chunkBlockBelongsTo->GetBlockIndexFromLocalCoords(localCoords + IntVec3(1, 0, 0));
	return blockIterator;
}

BlockIterator BlockIterator::GetNorthNeighbour() const
{
	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.y == CHUNK_MAX_Y)
	{
		blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo->m_northNeighbour;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(localCoords.x, 0, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo;
	blockIterator.m_blockIndex = m_chunkBlockBelongsTo->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, 1, 0));
	return blockIterator;
}

BlockIterator BlockIterator::GetWestNeighbour() const
{
	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.x == 0)
	{
		blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo->m_westNeighbour;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(CHUNK_MAX_X, localCoords.y, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo;
	blockIterator.m_blockIndex = m_chunkBlockBelongsTo->GetBlockIndexFromLocalCoords(localCoords + IntVec3(-1, 0, 0));
	return blockIterator;
}

BlockIterator BlockIterator::GetSouthNeighbour() const
{
	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.y == 0)
	{
		blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo->m_southNeighbour;
		blockIterator.m_blockIndex = Chunk::GetBlockIndexFromLocalCoords(IntVec3(localCoords.x, CHUNK_MAX_Y, localCoords.z));
		return blockIterator;
	}

	blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo;
	blockIterator.m_blockIndex = m_chunkBlockBelongsTo->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, -1, 0));
	return blockIterator;
}

BlockIterator BlockIterator::GetAboveNeighbour() const
{
	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.z == CHUNK_MAX_Z)
	{
		return *this;
	}

	blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo;
	blockIterator.m_blockIndex = m_chunkBlockBelongsTo->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, 0, 1));
	return blockIterator;
}

BlockIterator BlockIterator::GetBelowNeighbour() const
{
	BlockIterator blockIterator = {};
	IntVec3 localCoords = m_chunkBlockBelongsTo->GetLocalCoordsFromBlockIndex(m_blockIndex);
	if (localCoords.z == 0)
	{
		return *this;
	}

	blockIterator.m_chunkBlockBelongsTo = m_chunkBlockBelongsTo;
	blockIterator.m_blockIndex = m_chunkBlockBelongsTo->GetBlockIndexFromLocalCoords(localCoords + IntVec3(0, 0, -1));
	return blockIterator;
}


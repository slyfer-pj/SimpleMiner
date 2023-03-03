#pragma once
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/AABB3.hpp"

class Chunk;
struct Block;

struct BlockIterator
{
public:
	Chunk* m_chunkBlockBelongsTo = nullptr;
	int m_blockIndex = 0;

public:
	Block* GetBlock() const;
	Vec3 GetWorldCenter() const;
	AABB3 GetBlockBounds() const;
	bool IsBlockOpaque() const;

	BlockIterator GetEastNeighbour() const;
	BlockIterator GetNorthNeighbour() const;
	BlockIterator GetWestNeighbour() const;
	BlockIterator GetSouthNeighbour() const;
	BlockIterator GetAboveNeighbour() const;
	BlockIterator GetBelowNeighbour() const;


};
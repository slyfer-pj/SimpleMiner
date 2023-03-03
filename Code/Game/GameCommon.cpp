#include "Engine/Math/Vec2.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/GameCommon.hpp"

extern Renderer* g_theRenderer;

void DrawLine(const Vec2& startPoint, const Vec2& endPoint, Rgba8 color, float thicknessOfLine)
{
	float halfWidth = thicknessOfLine * 0.5f;
	Vec2 unitForwardVector = (endPoint - startPoint) / (endPoint - startPoint).GetLength();
	Vec2 unitLeftVector = unitForwardVector.GetRotated90Degrees();
	Vec2 leftTopCorner = startPoint + (halfWidth * unitLeftVector) - (halfWidth * unitForwardVector);
	Vec2 leftBottomCorner = startPoint - (halfWidth * unitLeftVector) - (halfWidth * unitForwardVector);
	Vec2 rightTopCorner = endPoint + (halfWidth * unitLeftVector) + (halfWidth * unitForwardVector);
	Vec2 rightBottomCorner = endPoint - (halfWidth * unitLeftVector) + (halfWidth * unitForwardVector);

	Vertex_PCU drawVerticesForLine[6];
	drawVerticesForLine[0].m_position = Vec3(leftTopCorner.x, leftTopCorner.y, 0.f);
	drawVerticesForLine[1].m_position = Vec3(leftBottomCorner.x, leftBottomCorner.y, 0.f);
	drawVerticesForLine[2].m_position = Vec3(rightTopCorner.x, rightTopCorner.y, 0.f);
	drawVerticesForLine[3].m_position = Vec3(leftBottomCorner.x, leftBottomCorner.y, 0.f);
	drawVerticesForLine[4].m_position = Vec3(rightBottomCorner.x, rightBottomCorner.y, 0.f);
	drawVerticesForLine[5].m_position = Vec3(rightTopCorner.x, rightTopCorner.y, 0.f);

	drawVerticesForLine[0].m_color = color;
	drawVerticesForLine[1].m_color = color;
	drawVerticesForLine[2].m_color = color;
	drawVerticesForLine[3].m_color = color;
	drawVerticesForLine[4].m_color = color;
	drawVerticesForLine[5].m_color = color;

	g_theRenderer->DrawVertexArray(6, drawVerticesForLine);
}

void DrawRing(const Vec2& ringCenter, float ringRadius, Rgba8 color, float ringThickness)
{
	float radiusOuter = ringRadius;
	float radiusInner = ringRadius - ringThickness;

	float thetaDeg = 0;
	float deltaThethaDeg = 360.f / 16.f;

	int vertIndex = 0;
	Vec2 pointsOnCircles[4];

	Vertex_PCU drawVerticesForRing[96];
	int drawVertIndex = 0;
	int vertexOrder[6] = { 0, 3, 1, 0, 2, 3 };

	while (thetaDeg <= 360.f)
	{
		if (vertIndex != 0 && vertIndex % 4 == 0)
		{
			for (int i = 0; i < 6; i++)
			{
				drawVerticesForRing[drawVertIndex].m_position = Vec3(pointsOnCircles[vertexOrder[i]].x + ringCenter.x
					, pointsOnCircles[vertexOrder[i]].y + ringCenter.y, 0);
				drawVerticesForRing[drawVertIndex].m_color = color;
				drawVertIndex++;
			}

			vertIndex = 0;
		}


		if (vertIndex != 0 && vertIndex % 2 == 0)
			thetaDeg += deltaThethaDeg;

		pointsOnCircles[vertIndex % 4].x = radiusOuter * CosDegrees(thetaDeg);
		pointsOnCircles[vertIndex % 4].y = radiusOuter * SinDegrees(thetaDeg);

		vertIndex++;

		pointsOnCircles[vertIndex % 4].x = radiusInner * CosDegrees(thetaDeg);
		pointsOnCircles[vertIndex % 4].y = radiusInner * SinDegrees(thetaDeg);

		vertIndex++;
	}

	g_theRenderer->DrawVertexArray(96, drawVerticesForRing);
}
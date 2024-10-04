// Fill out your copyright notice in the Description page of Project Settings.


#include "Chunk.h"
#include "FastNoiseLite.h"
#include "ProceduralMeshComponent.h"

// Sets default values
AChunk::AChunk()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>("Mesh");
	Noise = new FastNoiseLite();


	Mesh->SetCastShadow(false);

	SetRootComponent(Mesh);

	
}

void AChunk::GenerateVoxels()
{
	

	FVector Position = GetActorLocation() / VoxelSize;

	for (int x = 0; x <= ChunkSize.X; x++)
	{
		for (int y = 0; y <= ChunkSize.Y; y++)
		{
			for (int z = 0; z <= ChunkSize.Z; z++)
			{
				float Height = FVector::Dist(Position + (FVector(x, y, z)), PlanetCenter / VoxelSize);
				
				FVector Normalized = (Position + FVector(x, y, z) - (PlanetCenter / VoxelSize)).GetSafeNormal() * PlanetRadius;
				if (Height <= (PlanetRadius + Noise->GetNoise(Normalized.X / ZoomLevel, Normalized .Y / ZoomLevel, Normalized.Z / ZoomLevel) * NoiseScaler))
				{
					Voxels[GetVoxelIndex(x, y, z)] = EBlock::Stone;
				}
				else if(Height < PlanetRadius)
				{
					Voxels[GetVoxelIndex(x, y, z)] = EBlock::Water;
				}
				else
				{
					
					Voxels[GetVoxelIndex(x, y, z)] = EBlock::Air;
				}
			}
		
		}
	}
}

void AChunk::ApplyMesh() const
{
	Mesh->SetMaterial(0, Material);
	Mesh->CreateMeshSection(0, MeshData.Vertices,
		MeshData.Triangles,
		MeshData.Normals,
		MeshData.UV0,
		MeshData.Colors,
		TArray<FProcMeshTangent>(),
		true);

	Mesh->SetMaterial(1, MaterialWater);
	Mesh->CreateMeshSection(1, MeshDataWater.Vertices,
		MeshDataWater.Triangles,
		MeshDataWater.Normals,
		MeshDataWater.UV0,
		MeshDataWater.Colors,
		TArray<FProcMeshTangent>(),
		false);
}

void AChunk::RenderChunk()
{
	GenerateMesh();

	ApplyMesh();
}

void AChunk::SetVoxelTo(FVector VoxelPos, EBlock blockType)
{
	int Index = GetVoxelIndex(round(VoxelPos.X), round(VoxelPos.Y), round(VoxelPos.Z));
	if(Voxels[Index] != EBlock::Water)
		Voxels[Index] = blockType;
}

// Called when the game starts or when spawned
void AChunk::BeginPlay()
{
	Super::BeginPlay();

	Voxels.SetNum((ChunkSize.X + 1) * (ChunkSize.Y + 1) * (ChunkSize.Z + 1));
	Noise->SetSeed(2);
	Noise->SetFrequency(Frequency);
	Noise->SetNoiseType(FastNoiseLite::NoiseType_Perlin);
	Noise->SetFractalType(FastNoiseLite::FractalType_FBm);
	Noise->SetFractalOctaves(FractalOctaves);
	Noise->SetFractalLacunarity(FractalLacunarity);
	Noise->SetFractalGain(FractalGain);


	GenerateVoxels();

	RenderChunk();
	
}

void AChunk::GenerateMesh()
{
	VertexCountWater = 0;	//reset our variables
	VertexCountGround = 0;
	MeshData = FChunkMeshData();
	MeshDataWater = FChunkMeshData();

	if (SurfaceLevel > 0.0f) //triangles face correct direction based on surface level
	{
		TriangleOrder[0] = 0;
		TriangleOrder[1] = 1;
		TriangleOrder[2] = 2;
	}
	else
	{
		TriangleOrder[0] = 2;
		TriangleOrder[1] = 1;
		TriangleOrder[2] = 0;
	}

	float GroundCube[8] = {};
	float WaterCube[8] = {};
	int sand = 0;

	for (int x = 0; x < ChunkSize.X; x++)
	{
		for (int y = 0; y < ChunkSize.Y; y++)
		{
			for (int z = 0; z < ChunkSize.Z; z++)
			{
				sand = 0;
				for (int i = 0; i < 8; i++)
				{
					
					EBlock boxVector = Voxels[GetVoxelIndex(x + VertexOffset[i][0], y + VertexOffset[i][1], z + VertexOffset[i][2])];
					boxVector == EBlock::Stone || boxVector == EBlock::Sand ? GroundCube[i] = 1 : GroundCube[i] = 0;
					boxVector == EBlock::Water ? WaterCube[i] = 1 : WaterCube[i] = 0;
					//if (boxVector == EBlock::Sand || boxVector == EBlock::Water) sand++;
				}
				/*if (sand < 4)
				{
					March(x, y, z, GroundCube, MeshData, VertexCountGround, EBlock::Stone);
				}
				else
				{
					March(x, y, z, GroundCube, MeshData, VertexCountGround, EBlock::Sand);
				}*/
				March(x, y, z, GroundCube, MeshData, VertexCountGround, EBlock::Stone);
				March(x, y, z, WaterCube, MeshDataWater, VertexCountWater, EBlock::Water);
			}
		}
	}
}

int AChunk::GetVoxelIndex(int X, int Y, int Z) const
{
	return X * (ChunkSize.Y + 1) * (ChunkSize.Z + 1) + Y * (ChunkSize.Z + 1) + Z;
}

void AChunk::March(int X, int Y, int Z, const float Cube[8], FChunkMeshData& data, int& VertexIncrementer, EBlock BlockType)
{
	int VertexMask = 0;
	for (int i = 0; i < 8; i++) //set our vertex mask
	{
		if (Cube[i] <= SurfaceLevel)
			VertexMask |= 1 << i;
	}
	
	const int EdgeMask = CubeEdgeFlags[VertexMask]; //where we should draw edges
	if (EdgeMask == 0) return; //no edges to draw, we are done

	FVector EdgeVertex[12];
	for (int i = 0; i < 12; i++)
	{
		if ((EdgeMask & 1 << i) != 0)
		{
			EdgeVertex[i].X = X + (VertexOffset[EdgeConnection[i][0]][0] + .5 * EdgeDirection[i][0]); //.5 is our "interpolation" value (not interpolating)
			EdgeVertex[i].Y = Y + (VertexOffset[EdgeConnection[i][0]][1] + .5 * EdgeDirection[i][1]);
			EdgeVertex[i].Z = Z + (VertexOffset[EdgeConnection[i][0]][2] + .5 * EdgeDirection[i][2]);
			/*if (BlockType == EBlock::Water)
				EdgeVertex[i] -= (EdgeVertex[i] - PlanetCenter).GetSafeNormal() * 0.5;*/ //CAUSES PROBLEM with planet center = 0,0,0
		}
	}

	for (int i = 0; i < 5; i++)
	{
		if (TriangleConnectionTable[VertexMask][3 * i] < 0) break;

		FVector V1 = EdgeVertex[TriangleConnectionTable[VertexMask][3 * i]] * VoxelSize; //get a vertex of the triangle we should draw
		FVector V2 = EdgeVertex[TriangleConnectionTable[VertexMask][3 * i + 1]] * VoxelSize;
		FVector V3 = EdgeVertex[TriangleConnectionTable[VertexMask][3 * i + 2]] * VoxelSize;

		FVector Normal = FVector::CrossProduct(V2 - V1, V3 - V1);
		Normal.Normalize();

		data.Vertices.Add(V1);
		data.Vertices.Add(V2);
		data.Vertices.Add(V3);

		data.Triangles.Add(VertexIncrementer + TriangleOrder[0]);
		data.Triangles.Add(VertexIncrementer + TriangleOrder[1]);
		data.Triangles.Add(VertexIncrementer + TriangleOrder[2]);

		data.Normals.Add(Normal);
		data.Normals.Add(Normal);
		data.Normals.Add(Normal);

		if (BlockType == EBlock::Water)
		{
			data.Colors.Add(FColor(50, 100, 200));
			data.Colors.Add(FColor(50, 100, 200));
			data.Colors.Add(FColor(50, 100, 200));
		}
		else if(BlockType == EBlock::Sand)
		{
			data.Colors.Add(FColor(250, 250, 200));
			data.Colors.Add(FColor(250, 250, 200));
			data.Colors.Add(FColor(250, 250, 200));
		}
		else 
		{
			data.Colors.Add(FColor(100, 100, 100));
			data.Colors.Add(FColor(100, 100, 100));
			data.Colors.Add(FColor(100, 100, 100));
		}

		VertexIncrementer += 3;
	}

}




#include <base/system.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

#include <vector>

#define STB_RECT_PACK_IMPLEMENTATION
#include <engine/external/stb/stb_rect_pack.h>

#define MAP_PADDING_X 24
#define MAP_PADDING_Y 16
#define MAP_TILESIZE 32

struct SMapImage
{
	int m_Width;
	int m_Height;
	int m_External;
	char *m_pName;
	void *m_pData;
	int m_DataSize;
	int m_Index;
};

struct SLayer
{
	CMapItemLayer m_Layer;
	int m_Width;
	int m_Height;
	int m_Flags;

	int m_NumQuads;

	CColor m_Color;
	int m_ColorEnv;
	int m_ColorEnvOffset;

	int m_Image;
	void *m_pData;
	int m_DataSize;

	int m_aName[3];
};

struct SLayerGroup
{
	int m_OffsetX;
	int m_OffsetY;
	int m_ParallaxX;
	int m_ParallaxY;

	int m_UseClipping;
	int m_ClipX;
	int m_ClipY;
	int m_ClipW;
	int m_ClipH;

	int m_aName[3];

	std::vector<SLayer *> m_vLayers;
};

int SaveGameLayer(CDataFileWriter &DataFileOut, int Width, int Height, void *pTiles, int &LayerCount, int Type)
{
	if(!pTiles)
		return 0;

	CMapItemLayerTilemap Item;
	Item.m_Version = 3;

	Item.m_Layer.m_Version = 0;
	Item.m_Layer.m_Flags = 0;
	Item.m_Layer.m_Type = LAYERTYPE_TILES;

	Item.m_Color = {255, 255, 255, 255};
	Item.m_ColorEnv = -1;
	Item.m_ColorEnvOffset = 0;

	Item.m_Width = Width;
	Item.m_Height = Height;

	Item.m_Image = -1;
	Item.m_Tele = -1;
	Item.m_Speedup = -1;
	Item.m_Front = -1;
	Item.m_Switch = -1;
	Item.m_Tune = -1;

	void *pEmptyData = malloc(Width * Height * sizeof(CTile));
	mem_zero(pEmptyData, Width * Height * sizeof(CTile));

	if(Type == TILESLAYERFLAG_GAME)
	{
		Item.m_Flags = TILESLAYERFLAG_GAME;
		Item.m_Data = DataFileOut.AddData(Width * Height * sizeof(CTile), pTiles);
		StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Game");
	}
	else if(Type == TILESLAYERFLAG_FRONT)
	{
		Item.m_Flags = TILESLAYERFLAG_FRONT;
		Item.m_Data = DataFileOut.AddData(Width * Height * sizeof(CTile), pEmptyData);
		StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Front");
		Item.m_Front = DataFileOut.AddData(Width * Height * sizeof(CTile), pTiles);
	}
	else if(Type == TILESLAYERFLAG_TUNE)
	{
		Item.m_Flags = TILESLAYERFLAG_TUNE;
		Item.m_Data = DataFileOut.AddData(Width * Height * sizeof(CTile), pEmptyData);
		StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Tune");
		Item.m_Tune = DataFileOut.AddData(Width * Height * sizeof(CTuneTile), pTiles);
	}
	else if(Type == TILESLAYERFLAG_TELE)
	{
		Item.m_Flags = TILESLAYERFLAG_TELE;
		Item.m_Data = DataFileOut.AddData(Width * Height * sizeof(CTile), pEmptyData);
		StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Tele");
		Item.m_Tele = DataFileOut.AddData(Width * Height * sizeof(CTeleTile), pTiles);
	}
	else if(Type == TILESLAYERFLAG_SPEEDUP)
	{
		Item.m_Flags = TILESLAYERFLAG_SPEEDUP;
		Item.m_Data = DataFileOut.AddData(Width * Height * sizeof(CTile), pEmptyData);
		StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Speedup");
		Item.m_Speedup = DataFileOut.AddData(Width * Height * sizeof(CSpeedupTile), pTiles);
	}
	else if(Type == TILESLAYERFLAG_SWITCH)
	{
		Item.m_Flags = TILESLAYERFLAG_SWITCH;
		Item.m_Data = DataFileOut.AddData(Width * Height * sizeof(CTile), pEmptyData);
		StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Switch");
		Item.m_Switch = DataFileOut.AddData(Width * Height * sizeof(CSwitchTile), pTiles);
	}
	else
	{
		free(pEmptyData);
		return 0;
	}

	dbg_msg("map_merge", "saving layer %d (%dx%d) type=%d, flags=%d, img=%d", LayerCount, Item.m_Width, Item.m_Height, Item.m_Layer.m_Type, Item.m_Flags, Item.m_Image);
	DataFileOut.AddItem(MAPITEMTYPE_LAYER, LayerCount++, sizeof(Item), &Item);
	free(pEmptyData);
	return 1;
}

int SaveLayers(CDataFileWriter &DataFileOut, std::vector<SLayer *> &vLayers, int &LayerCount)
{
	int NumGroupLayer = 0;
	for(auto &pLayer : vLayers)
	{
		if(pLayer->m_Layer.m_Type == LAYERTYPE_TILES)
		{
			CMapItemLayerTilemap Item;
			Item.m_Version = 3;

			Item.m_Layer.m_Version = 0;
			Item.m_Layer.m_Flags = pLayer->m_Layer.m_Flags;
			Item.m_Layer.m_Type = pLayer->m_Layer.m_Type;

			Item.m_Color = pLayer->m_Color;
			Item.m_ColorEnv = pLayer->m_ColorEnv;
			Item.m_ColorEnvOffset = pLayer->m_ColorEnvOffset;

			Item.m_Width = pLayer->m_Width;
			Item.m_Height = pLayer->m_Height;
			Item.m_Flags = pLayer->m_Flags;

			Item.m_Image = pLayer->m_Image;

			// TODO: ddnet layers
			Item.m_Tele = -1;
			Item.m_Speedup = -1;
			Item.m_Front = -1;
			Item.m_Switch = -1;
			Item.m_Tune = -1;

			Item.m_Data = DataFileOut.AddData(pLayer->m_DataSize, pLayer->m_pData);

			Item.m_aName[0] = pLayer->m_aName[0];
			Item.m_aName[1] = pLayer->m_aName[1];
			Item.m_aName[2] = pLayer->m_aName[2];

			dbg_msg("map_merge", "saving tile layer %d (%dx%d) flags=%d, img=%d", LayerCount, Item.m_Width, Item.m_Height, Item.m_Flags, Item.m_Image);
			DataFileOut.AddItem(MAPITEMTYPE_LAYER, LayerCount++, sizeof(Item), &Item);
			// automapper is skipped, since it won't work afterwards anyway
			NumGroupLayer++;
		}
		else if(pLayer->m_Layer.m_Type == LAYERTYPE_QUADS)
		{
			CMapItemLayerQuads Item;
			Item.m_Version = 2;
			Item.m_Layer.m_Version = 0;
			Item.m_Layer.m_Flags = pLayer->m_Layer.m_Flags;
			Item.m_Layer.m_Type = pLayer->m_Layer.m_Type;
			Item.m_Image = pLayer->m_Image;

			Item.m_aName[0] = pLayer->m_aName[0];
			Item.m_aName[1] = pLayer->m_aName[1];
			Item.m_aName[2] = pLayer->m_aName[2];

			Item.m_NumQuads = pLayer->m_NumQuads;
			Item.m_Data = DataFileOut.AddData(pLayer->m_DataSize, pLayer->m_pData);

			dbg_msg("map_merge", "saving quad layer %d, img=%d, num_quads=%d", LayerCount, Item.m_Image, Item.m_NumQuads);
			DataFileOut.AddItem(MAPITEMTYPE_LAYER, LayerCount++, sizeof(Item), &Item);
			NumGroupLayer++;
		}
	}
	return NumGroupLayer;
}

void SaveGroups(CDataFileWriter &DataFileOut, std::vector<SLayerGroup *> &Groups, int &LayerCount, int &GroupCount)
{
	for(auto &pGroup : Groups)
	{
		CMapItemGroup GItem;
		GItem.m_Version = CMapItemGroup::CURRENT_VERSION;
		GItem.m_ParallaxX = pGroup->m_ParallaxX;
		GItem.m_ParallaxY = pGroup->m_ParallaxY;
		GItem.m_OffsetX = pGroup->m_OffsetX;
		GItem.m_OffsetY = pGroup->m_OffsetY;
		GItem.m_UseClipping = pGroup->m_UseClipping;
		GItem.m_ClipX = pGroup->m_ClipX;
		GItem.m_ClipY = pGroup->m_ClipY;
		GItem.m_ClipW = pGroup->m_ClipW;
		GItem.m_ClipH = pGroup->m_ClipH;
		GItem.m_aName[0] = pGroup->m_aName[0];
		GItem.m_aName[1] = pGroup->m_aName[1];
		GItem.m_aName[2] = pGroup->m_aName[2];
		GItem.m_StartLayer = LayerCount;
		GItem.m_NumLayers = SaveLayers(DataFileOut, pGroup->m_vLayers, LayerCount);
		dbg_msg("map_merge", "saving group %d containing layers %d-%d, isclip=%d, off=%d:%d, para=%d:%d, clip=%d:%d:%d:%d", GroupCount, GItem.m_StartLayer, GItem.m_StartLayer + GItem.m_NumLayers - 1, GItem.m_UseClipping, GItem.m_OffsetX, GItem.m_OffsetY, GItem.m_ParallaxX, GItem.m_ParallaxY, GItem.m_ClipX, GItem.m_ClipY, GItem.m_ClipW, GItem.m_ClipH);
		DataFileOut.AddItem(MAPITEMTYPE_GROUP, GroupCount++, sizeof(GItem), &GItem);
	}
}

bool Process(IStorage *pStorage, char *pOutName, char **pMapNames, int NumMaps)
{
	// Preload data & packing
	CDataFileReader **ppDataFiles = (CDataFileReader **)malloc(NumMaps * sizeof(CDataFileReader *));
	stbrp_rect *pRects = (stbrp_rect *)malloc(NumMaps * sizeof(stbrp_rect));

	int MaxMapWidth = 0;
	for(int MapIndex = 0; MapIndex < NumMaps; MapIndex++)
	{
		ppDataFiles[MapIndex] = new CDataFileReader();
		CDataFileReader &DataFile = *ppDataFiles[MapIndex];

		if(!DataFile.Open(pStorage, pMapNames[MapIndex], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_merge", "error opening map '%s'", pMapNames[MapIndex]);
			return false;
		}

		int LayersStart, LayersNum;
		DataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);
		int MapWidth = 0;
		int MapHeight = 0;
		for(int i = 0; i < LayersNum; i++)
		{
			CMapItemLayer *pLayerItem = (CMapItemLayer *)DataFile.GetItem(LayersStart + i, 0, 0);
			if(pLayerItem->m_Type != LAYERTYPE_TILES)
				continue;

			CMapItemLayerTilemap *pTilemapItem = (CMapItemLayerTilemap *)pLayerItem;
			if(pTilemapItem->m_Flags)
			{
				MapWidth = maximum(MapWidth, pTilemapItem->m_Width);
				MapHeight = maximum(MapHeight, pTilemapItem->m_Height);
			}
		}

		pRects[MapIndex].id = 0;
		pRects[MapIndex].w = MapWidth + MAP_PADDING_X * 2;
		pRects[MapIndex].h = MapHeight + MAP_PADDING_Y * 2;
		pRects[MapIndex].was_packed = false;

		MaxMapWidth = maximum(MapWidth + MAP_PADDING_X * 2, MaxMapWidth);
		dbg_msg("map_merge", "size of map %s: %dx%d", pMapNames[MapIndex], MapWidth, MapHeight);
	}

	int MaxWidth = 2;
	while(MaxWidth < MaxMapWidth)
		MaxWidth *= 2;

	dbg_msg("map_merge", "packing maps into a mega map of width %d", MaxWidth);

	stbrp_node *apPackerStorage = (stbrp_node *)malloc(sizeof(stbrp_node) * MaxWidth);
	stbrp_context Packer;
	stbrp_init_target(&Packer, MaxWidth, 10000, apPackerStorage, MaxWidth);

	bool PackingSuccess = stbrp_pack_rects(&Packer, pRects, NumMaps);
	free(apPackerStorage);
	if(!PackingSuccess)
	{
		dbg_msg("map_merge", "error packing maps");
		return false;
	}

	MaxMapWidth = 0;
	int MaxMapHeight = 0;
	for(int MapIndex = 0; MapIndex < NumMaps; MapIndex++)
	{
		dbg_msg("map_merge", "%s at %d, %d", pMapNames[MapIndex], pRects[MapIndex].x, pRects[MapIndex].y);

		MaxMapWidth = maximum(pRects[MapIndex].x + pRects[MapIndex].w - MAP_PADDING_X * 2, MaxMapWidth);
		MaxMapHeight = maximum(pRects[MapIndex].y + pRects[MapIndex].h - MAP_PADDING_Y * 2, MaxMapHeight);
	}

	dbg_msg("map_merge", "mega map final size: %d x %d", MaxMapWidth, MaxMapHeight);

	std::vector<SLayerGroup *> PreGameGroupGroups; // store groups before game groups
	std::vector<SLayerGroup *> PostGameGroupGroups; // store groups after game groups

	// Game Group layers
	CTile *pGameTiles = (CTile *)malloc(MaxMapWidth * MaxMapHeight * sizeof(CTile));
	mem_zero(pGameTiles, MaxMapWidth * MaxMapHeight * sizeof(CTile));
	CTeleTile *pTeleTiles = nullptr;
	CSpeedupTile *pSpeedupTiles = (CSpeedupTile *)malloc(MaxMapWidth * MaxMapHeight * sizeof(CSpeedupTile));
	CTile *pFrontTiles = nullptr;
	CSwitchTile *pSwitchTiles = nullptr;
	CTuneTile *pTuneTiles = nullptr;
	mem_zero(pSpeedupTiles, MaxMapWidth * MaxMapHeight * sizeof(CSpeedupTile));

	int NumImages = 0;
	int NumSounds = 0;

	std::vector<SMapImage> MapImages;
	std::vector<CEnvPoint> EnvPoints;
	std::vector<CMapItemEnvelope> Envelopes;

	CDataFileWriter DataFileOut;
	if(!DataFileOut.Open(pStorage, pOutName))
	{
		dbg_msg("map_merge", "failed to open file '%s'...", pOutName);
		return false;
	}

	// merge maps
	for(int MapIndex = 0; MapIndex < NumMaps; MapIndex++)
	{
		CDataFileReader &DataFile = *ppDataFiles[MapIndex];

		// load images
		std::vector<int> ImageIDMap;
		{
			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_IMAGE, &Start, &Num);
			ImageIDMap.resize(Num);

			for(int i = 0; i < Num; i++)
			{
				CMapItemImage *pItem = (CMapItemImage *)DataFile.GetItem(Start + i, 0, 0);
				char *pName = (char *)DataFile.GetData(pItem->m_ImageName);

				bool ImageAlreadyExists = false;
				for(auto &Image : MapImages)
				{
					if(Image.m_External == pItem->m_External && str_comp(Image.m_pName, pName) == 0)
					{
						ImageIDMap[i] = Image.m_Index;
						ImageAlreadyExists = true;
						break;
					}
				}

				if(ImageAlreadyExists)
				{
					// save memory (probably)
					DataFile.UnloadData(pItem->m_ImageName);
					DataFile.UnloadData(pItem->m_ImageData);
					continue;
				}

				SMapImage Image;
				Image.m_External = pItem->m_External;
				Image.m_Width = pItem->m_Width;
				Image.m_Height = pItem->m_Height;
				Image.m_pName = pName;
				if(Image.m_External)
					Image.m_pData = nullptr;
				else
				{
					Image.m_pData = DataFile.GetData(pItem->m_ImageData);
					Image.m_DataSize = DataFile.GetDataSize(pItem->m_ImageData);
				}

				Image.m_Index = NumImages++;
				ImageIDMap[i] = Image.m_Index;

				MapImages.push_back(Image);
			}
		}

		int StartEnvelope = Envelopes.size();
		// load envelops
		{
			CEnvPoint *pPoints = nullptr;
			{
				int Start, Num;
				DataFile.GetType(MAPITEMTYPE_ENVPOINTS, &Start, &Num);
				if(Num)
					pPoints = (CEnvPoint *)DataFile.GetItem(Start, 0, 0);
			}

			int Start, Num;
			DataFile.GetType(MAPITEMTYPE_ENVELOPE, &Start, &Num);
			for(int e = 0; e < Num; e++)
			{
				CMapItemEnvelope *pItem = (CMapItemEnvelope *)DataFile.GetItem(Start + e, 0, 0);
				CMapItemEnvelope Item;
				Item.m_Version = CMapItemEnvelope::CURRENT_VERSION;
				Item.m_Channels = pItem->m_Channels;
				Item.m_StartPoint = EnvPoints.size();
				Item.m_NumPoints = pItem->m_NumPoints;
				if(pItem->m_aName[0] != -1)
					StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), pMapNames[MapIndex]);
				else
					StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "");

				Item.m_Synchronized = pItem->m_Version >= 2 ? pItem->m_Synchronized : false;
				Envelopes.push_back(Item);

				for(int i = pItem->m_StartPoint; i < pItem->m_StartPoint + pItem->m_NumPoints; i++)
					EnvPoints.push_back(pPoints[i]);
			}
		}

		// load groups
		int LayersStart, LayersNum;
		DataFile.GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

		int Start, Num;
		DataFile.GetType(MAPITEMTYPE_GROUP, &Start, &Num);

		int MapPositionX = pRects[MapIndex].x;
		int MapPositionY = pRects[MapIndex].y;
		int MapPositionW = pRects[MapIndex].w - MAP_PADDING_X * 2;
		int MapPositionH = pRects[MapIndex].h - MAP_PADDING_Y * 2;

		auto *pCurrentGroupGroup = &PreGameGroupGroups;

		for(int g = 0; g < Num; g++)
		{
			CMapItemGroup *pGItem = (CMapItemGroup *)DataFile.GetItem(Start + g, 0, 0);

			if(pGItem->m_Version < 1 || pGItem->m_Version > CMapItemGroup::CURRENT_VERSION)
				continue;

			SLayerGroup *pLayerGroup = new SLayerGroup();
			pLayerGroup->m_ParallaxX = pGItem->m_ParallaxX;
			pLayerGroup->m_ParallaxY = pGItem->m_ParallaxY;
			pLayerGroup->m_OffsetX = pGItem->m_OffsetX - (MapPositionX * pGItem->m_ParallaxX / (float)100) * MAP_TILESIZE;
			pLayerGroup->m_OffsetY = pGItem->m_OffsetY - (MapPositionY * pGItem->m_ParallaxY / (float)100) * MAP_TILESIZE;

			pLayerGroup->m_UseClipping = true;
			if(pGItem->m_Version >= 2 && pGItem->m_UseClipping)
			{
				pLayerGroup->m_ClipX = pGItem->m_ClipX;
				pLayerGroup->m_ClipY = pGItem->m_ClipY;
				pLayerGroup->m_ClipW = pGItem->m_ClipW;
				pLayerGroup->m_ClipH = pGItem->m_ClipH;
			}
			else
			{
				pLayerGroup->m_ClipX = -MAP_PADDING_X * MAP_TILESIZE;
				pLayerGroup->m_ClipY = -MAP_PADDING_Y * MAP_TILESIZE;
				pLayerGroup->m_ClipW = (MapPositionW + MAP_PADDING_X * 2) * MAP_TILESIZE;
				pLayerGroup->m_ClipH = (MapPositionH + MAP_PADDING_Y * 2) * MAP_TILESIZE;
			}

			pLayerGroup->m_ClipX += MapPositionX * MAP_TILESIZE;
			pLayerGroup->m_ClipY += MapPositionY * MAP_TILESIZE;

			if(pGItem->m_Version >= 3)
			{
				pLayerGroup->m_aName[0] = pGItem->m_aName[0];
				pLayerGroup->m_aName[1] = pGItem->m_aName[1];
				pLayerGroup->m_aName[2] = pGItem->m_aName[2];
			}
			else
			{
				StrToInts(pLayerGroup->m_aName, sizeof(pLayerGroup->m_aName) / sizeof(int), pMapNames[MapIndex]);
			}

			for(int l = 0; l < pGItem->m_NumLayers; l++)
			{
				CMapItemLayer *pLayerItem = (CMapItemLayer *)DataFile.GetItem(LayersStart + pGItem->m_StartLayer + l, 0, 0);
				if(!pLayerItem)
					continue;

				if(pLayerItem->m_Type == LAYERTYPE_TILES)
				{
					bool IsGameLayer = false;
					CMapItemLayerTilemap *pTilemapItem = (CMapItemLayerTilemap *)pLayerItem;
					if(pTilemapItem->m_Flags)
					{
						if(pTilemapItem->m_Flags & TILESLAYERFLAG_GAME)
						{
							if(pLayerGroup->m_vLayers.size() > 0)
							{
								pCurrentGroupGroup->push_back(pLayerGroup);

								SLayerGroup *pNewLayerGroup = new SLayerGroup();
								pNewLayerGroup->m_OffsetX = pLayerGroup->m_OffsetX;
								pNewLayerGroup->m_OffsetY = pLayerGroup->m_OffsetY;
								pNewLayerGroup->m_ParallaxX = pLayerGroup->m_ParallaxX;
								pNewLayerGroup->m_ParallaxY = pLayerGroup->m_ParallaxY;
								pNewLayerGroup->m_UseClipping = pLayerGroup->m_UseClipping;
								pNewLayerGroup->m_ClipX = pLayerGroup->m_ClipX;
								pNewLayerGroup->m_ClipY = pLayerGroup->m_ClipY;
								pNewLayerGroup->m_ClipW = pLayerGroup->m_ClipW;
								pNewLayerGroup->m_ClipH = pLayerGroup->m_ClipH;
								pNewLayerGroup->m_aName[0] = pLayerGroup->m_aName[0];
								pNewLayerGroup->m_aName[1] = pLayerGroup->m_aName[1];
								pNewLayerGroup->m_aName[2] = pLayerGroup->m_aName[2];
								pLayerGroup = pNewLayerGroup;
							}
							pCurrentGroupGroup = &PostGameGroupGroups;
						}
						IsGameLayer = true;
					}

					SLayer *pLayer = new SLayer();
					pLayer->m_pData = DataFile.GetData(pTilemapItem->m_Data);
					pLayer->m_DataSize = DataFile.GetDataSize(pTilemapItem->m_Data);
					pLayer->m_Layer.m_Flags = pTilemapItem->m_Layer.m_Flags;
					pLayer->m_Layer.m_Type = pTilemapItem->m_Layer.m_Type;
					pLayer->m_Width = pTilemapItem->m_Width;
					pLayer->m_Height = pTilemapItem->m_Height;
					pLayer->m_Color = pTilemapItem->m_Color;
					pLayer->m_ColorEnv = pTilemapItem->m_ColorEnv < 0 ? -1 : StartEnvelope + pTilemapItem->m_ColorEnv;
					pLayer->m_ColorEnvOffset = pTilemapItem->m_ColorEnvOffset;
					pLayer->m_Flags = pTilemapItem->m_Flags;
					pLayer->m_Image = pTilemapItem->m_Image >= 0 ? ImageIDMap[pTilemapItem->m_Image] : -1;

					if(pTilemapItem->m_Layer.m_Version >= 3)
					{
						pLayer->m_aName[0] = pTilemapItem->m_aName[0];
						pLayer->m_aName[1] = pTilemapItem->m_aName[1];
						pLayer->m_aName[2] = pTilemapItem->m_aName[2];
					}
					else
					{
						StrToInts(pLayer->m_aName, sizeof(pLayer->m_aName) / sizeof(int), "");
					}

					if(pTilemapItem->m_Version > 3)
					{
						// extract skipped tiles (0.7)
						CTile *pTiles = (CTile *)pLayer->m_pData;
						CTile *pNewTiles = (CTile *)malloc(sizeof(CTile) * pLayer->m_Width * pLayer->m_Height);
						int i = 0;
						while(i < pLayer->m_Width * pLayer->m_Height)
						{
							for(unsigned Counter = 0; Counter <= pTiles->m_Skip && i < pLayer->m_Width * pLayer->m_Height; Counter++)
							{
								pNewTiles[i] = *pTiles;
								pNewTiles[i++].m_Skip = 0;
							}
							pTiles++;
						}
						pLayer->m_pData = pNewTiles;
						pLayer->m_DataSize = sizeof(CTile) * pLayer->m_Width * pLayer->m_Height;

						// convert 0.7 unhookable
						if(pLayer->m_Image >= 0)
						{
							auto Image = MapImages[pLayer->m_Image];
							if(str_comp(Image.m_pName, "generic_unhookable") == 0)
							{
								CTile *pTiles = (CTile *)pLayer->m_pData;
								for(int y = 0; y < pLayer->m_Height; y++)
									for(int x = 0; x < pLayer->m_Width; x++)
									{
										CTile *pThisTile = &pTiles[y * pLayer->m_Width + x];
										if(pThisTile->m_Index == 38 || pThisTile->m_Index == 45 || pThisTile->m_Index == 166)
											pThisTile->m_Index -= 37;

										if(pThisTile->m_Index == 54 || pThisTile->m_Index == 61 || pThisTile->m_Index == 182)
											pThisTile->m_Index -= 36;

										if(pThisTile->m_Index == 70 || pThisTile->m_Index == 77 || pThisTile->m_Index == 198)
											pThisTile->m_Index -= 36;

										if(pThisTile->m_Index == 86 || pThisTile->m_Index == 93 || pThisTile->m_Index == 214)
											pThisTile->m_Index -= 52;

										if(pThisTile->m_Index >= 99 && pThisTile->m_Index <= 101 ||
											pThisTile->m_Index >= 115 && pThisTile->m_Index <= 117 ||
											pThisTile->m_Index >= 106 && pThisTile->m_Index <= 108 ||
											pThisTile->m_Index >= 122 && pThisTile->m_Index <= 124 ||
											pThisTile->m_Index >= 227 && pThisTile->m_Index <= 229 ||
											pThisTile->m_Index >= 243 && pThisTile->m_Index <= 245)
											pThisTile->m_Index -= 32;
									}
							}
						}
					}

					if(!IsGameLayer)
						pLayerGroup->m_vLayers.push_back(pLayer);
					else
					{
						if(pTilemapItem->m_Flags & TILESLAYERFLAG_GAME)
						{
							CTile *pTiles = (CTile *)pLayer->m_pData;
							for(int y = -MAP_PADDING_Y; y < pLayer->m_Height + MAP_PADDING_Y; y++)
								for(int x = -MAP_PADDING_X; x < pLayer->m_Width + MAP_PADDING_X; x++)
								{
									int MapX = MapPositionX + x;
									int MapY = MapPositionY + y;
									int TargetIndex = MapY * MaxMapWidth + MapX;

									if(MapX < 0 || MapX >= MaxMapWidth || MapY < 0 || MapY >= MaxMapHeight)
										continue;

									if(x == -MAP_PADDING_X || y == -MAP_PADDING_Y || x == pLayer->m_Width + MAP_PADDING_X - 1 || y == pLayer->m_Height + MAP_PADDING_Y - 1)
									{
										pGameTiles[TargetIndex].m_Index = TILE_NOHOOK;
										pGameTiles[TargetIndex].m_Flags = 0;
										pGameTiles[TargetIndex].m_Reserved = 0;
										pGameTiles[TargetIndex].m_Skip = 0;
									}
									else if(x < 0 || y < 0 || x >= pLayer->m_Width || y >= pLayer->m_Height)
									{
										int TargetX = 0;
										int TargetY = 0;
										if(x < 0)
											TargetX = 0;
										if(x >= pLayer->m_Width)
											TargetX = pLayer->m_Width - 1;
										if(y < 0)
											TargetY = 0;
										if(y >= pLayer->m_Height)
											TargetY = pLayer->m_Height - 1;
										pGameTiles[TargetIndex] = pTiles[TargetY * pLayer->m_Width + TargetX];
									}
									else
									{
										CTile Tile = pTiles[y * pLayer->m_Width + x];
										pGameTiles[TargetIndex] = Tile;
										if(Tile.m_Index >= ENTITY_OFFSET)
										{
											pSpeedupTiles[TargetIndex].m_Type = TILE_MEGAMAP_INDEX;
											pSpeedupTiles[TargetIndex].m_Force = 0;
											pSpeedupTiles[TargetIndex].m_MaxSpeed = MapIndex + 1;
										}
									}
								}
						}
						free(pLayer);
					};
				}
				else if(pLayerItem->m_Type == LAYERTYPE_QUADS)
				{
					CMapItemLayerQuads *pQuadsItem = (CMapItemLayerQuads *)pLayerItem;
					SLayer *pLayer = new SLayer();
					pLayer->m_pData = DataFile.GetData(pQuadsItem->m_Data);
					pLayer->m_DataSize = DataFile.GetDataSize(pQuadsItem->m_Data);
					pLayer->m_Layer.m_Flags = pQuadsItem->m_Layer.m_Flags;
					pLayer->m_Layer.m_Type = pQuadsItem->m_Layer.m_Type;
					pLayer->m_NumQuads = pQuadsItem->m_NumQuads;
					pLayer->m_Image = pQuadsItem->m_Image >= 0 ? ImageIDMap[pQuadsItem->m_Image] : -1;

					CQuad *pQuads = (CQuad *)pLayer->m_pData;
					for(int i = 0; i < pLayer->m_NumQuads; i++)
					{
						pQuads[i].m_ColorEnv += pQuads[i].m_ColorEnv < 0 ? 0 : StartEnvelope;
						pQuads[i].m_PosEnv += pQuads[i].m_PosEnv < 0 ? 0 : StartEnvelope;
					}

					if(pQuadsItem->m_Layer.m_Version >= 2)
					{
						pLayer->m_aName[0] = pQuadsItem->m_aName[0];
						pLayer->m_aName[1] = pQuadsItem->m_aName[1];
						pLayer->m_aName[2] = pQuadsItem->m_aName[2];
					}
					else
					{
						StrToInts(pLayer->m_aName, sizeof(pLayer->m_aName) / sizeof(int), "");
					}

					pLayerGroup->m_vLayers.push_back(pLayer);
				}
			}

			if(pLayerGroup->m_vLayers.size() > 0)
				pCurrentGroupGroup->push_back(pLayerGroup);
		}
	}

	// save map
	{
		// save version
		CMapItemVersion Item;
		Item.m_Version = 1;
		DataFileOut.AddItem(MAPITEMTYPE_VERSION, 0, sizeof(Item), &Item);
	}
	{
		// save info
		// TODO: merge credits?
		CMapItemInfoSettings Item;
		Item.m_Version = 1;
		Item.m_Author = -1;
		Item.m_MapVersion = -1;
		Item.m_Credits = -1;
		Item.m_License = -1;
		Item.m_Settings = -1;
		DataFileOut.AddItem(MAPITEMTYPE_INFO, 0, sizeof(Item), &Item);
	}

	{
		// save images
		for(auto &Image : MapImages)
		{
			CMapItemImage Item;
			Item.m_Version = 1;

			Item.m_Width = Image.m_Width;
			Item.m_Height = Image.m_Height;
			Item.m_External = Image.m_External;
			Item.m_ImageName = DataFileOut.AddData(str_length(Image.m_pName) + 1, Image.m_pName);
			if(Image.m_External)
				Item.m_ImageData = -1;
			else
				Item.m_ImageData = DataFileOut.AddData(Image.m_DataSize, Image.m_pData);
			dbg_msg("map_merge", "saving image %d (%dx%d), external=%d, name=%s", Image.m_Index, Image.m_Width, Image.m_Height, Image.m_External, Image.m_pName);
			DataFileOut.AddItem(MAPITEMTYPE_IMAGE, Image.m_Index, sizeof(Item), &Item);
		}
	}

	{
		// save envelopes
		for(unsigned int e = 0; e < Envelopes.size(); e++)
			DataFileOut.AddItem(MAPITEMTYPE_ENVELOPE, e, sizeof(CMapItemEnvelope), &Envelopes[e]);

		// save points
		if(EnvPoints.size() < 1)
			EnvPoints.push_back({0});

		int TotalSize = sizeof(CEnvPoint) * EnvPoints.size();
		DataFileOut.AddItem(MAPITEMTYPE_ENVPOINTS, 0, TotalSize, EnvPoints.data());
	}

	{
		// save groups
		int LayerCount = 0;
		int GroupCount = 0;

		// Game group
		SaveGroups(DataFileOut, PreGameGroupGroups, LayerCount, GroupCount);

		CMapItemGroup GameGroup;
		StrToInts(GameGroup.m_aName, sizeof(GameGroup.m_aName) / sizeof(int), "Game");
		GameGroup.m_Version = CMapItemGroup::CURRENT_VERSION;
		GameGroup.m_ParallaxX = 100;
		GameGroup.m_ParallaxY = 100;
		GameGroup.m_OffsetX = 0;
		GameGroup.m_OffsetY = 0;
		GameGroup.m_UseClipping = false;
		GameGroup.m_ClipX = 0;
		GameGroup.m_ClipY = 0;
		GameGroup.m_ClipW = 0;
		GameGroup.m_ClipH = 0;

		GameGroup.m_StartLayer = LayerCount;
		GameGroup.m_NumLayers = 0;
		GameGroup.m_NumLayers += SaveGameLayer(DataFileOut, MaxMapWidth, MaxMapHeight, pGameTiles, LayerCount, TILESLAYERFLAG_GAME);
		GameGroup.m_NumLayers += SaveGameLayer(DataFileOut, MaxMapWidth, MaxMapHeight, pTeleTiles, LayerCount, TILESLAYERFLAG_TELE);
		GameGroup.m_NumLayers += SaveGameLayer(DataFileOut, MaxMapWidth, MaxMapHeight, pSpeedupTiles, LayerCount, TILESLAYERFLAG_SPEEDUP);
		GameGroup.m_NumLayers += SaveGameLayer(DataFileOut, MaxMapWidth, MaxMapHeight, pFrontTiles, LayerCount, TILESLAYERFLAG_FRONT);
		GameGroup.m_NumLayers += SaveGameLayer(DataFileOut, MaxMapWidth, MaxMapHeight, pSwitchTiles, LayerCount, TILESLAYERFLAG_SWITCH);
		GameGroup.m_NumLayers += SaveGameLayer(DataFileOut, MaxMapWidth, MaxMapHeight, pTuneTiles, LayerCount, TILESLAYERFLAG_TUNE);
		dbg_msg("map_merge", "saving group %d containing layers %d-%d, isclip=%d, off=%d:%d, para=%d:%d, clip=%d:%d:%d:%d", GroupCount, GameGroup.m_StartLayer, GameGroup.m_StartLayer + GameGroup.m_NumLayers - 1, GameGroup.m_UseClipping, GameGroup.m_OffsetX, GameGroup.m_OffsetY, GameGroup.m_ParallaxX, GameGroup.m_ParallaxY, GameGroup.m_ClipX, GameGroup.m_ClipY, GameGroup.m_ClipW, GameGroup.m_ClipH);
		DataFileOut.AddItem(MAPITEMTYPE_GROUP, GroupCount++, sizeof(GameGroup), &GameGroup);

		SaveGroups(DataFileOut, PostGameGroupGroups, LayerCount, GroupCount);
		DataFileOut.Finish();
	}

	// free data
	for(int i = 0; i < NumMaps; i++)
	{
		ppDataFiles[i]->Close();
		delete ppDataFiles[i];
	}

	free(ppDataFiles);
	free(pRects);
	return true;
}

int main(int argc, char *argv[])
{
	dbg_logger_stdout();

	IStorage *pStorage = CreateLocalStorage();

	if(argc > 3)
	{
		return Process(pStorage, argv[1], &argv[2], argc - 2) ? 0 : 1;
	}
	else
	{
		dbg_msg("usage", "%s outmap map1 map2 ...", argv[0]);
		return -1;
	}
}

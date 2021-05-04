#include <base/system.h>
#include <engine/external/stb/stb_rect_pack.h>
#include <engine/shared/datafile.h>
#include <engine/storage.h>
#include <game/gamecore.h>
#include <game/mapitems.h>

#include <vector>

struct SMapImage
{
	int m_Width;
	int m_Height;
	int m_External;
	char *m_pName;
	void *m_pData;
	int m_Index;
};

struct SLayer
{
	char m_aName[12];
	int m_Type;
	int m_Flags;

	bool m_Readonly;
	bool m_Visible;

	int m_BrushRefCount;
};

struct SLayerGroup
{
	int m_Version;
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

bool Process(IStorage *pStorage, char *pOutName, char **pMapNames, int NumMaps)
{
	std::vector<SLayerGroup *> PreGameGroupGroups; // store groups before game groups
	std::vector<SLayerGroup *> PreGameLayerGroups; // store new groups grouping layers in game group before game layer
	std::vector<SLayerGroup *> PostGameLayerGroups; // store new groups grouping layers in game group after game layer
	std::vector<SLayerGroup *> PostGameGroupGroups; // store groups after game groups

	int NumImages = 0;
	int NumSounds = 0;

	std::vector<SMapImage> MapImages;

	CDataFileWriter DataFileOut;
	if(!DataFileOut.Open(pStorage, pOutName))
	{
		dbg_msg("map_merge", "failed to open file '%s'...", pOutName);
		return false;
	}

	std::vector<CDataFileReader *> DataFiles;

	// merge maps

	for(int MapIndex = 0; MapIndex < NumMaps; MapIndex++)
	{
		CDataFileReader *pDataFile = new CDataFileReader();

		if(!pDataFile->Open(pStorage, pMapNames[MapIndex], IStorage::TYPE_ABSOLUTE))
		{
			dbg_msg("map_merge", "error opening map '%s'", pMapNames[MapIndex]);
			return false;
		}

		// load images
		std::vector<int> ImageIDMap;
		{
			int Start, Num;
			pDataFile->GetType(MAPITEMTYPE_IMAGE, &Start, &Num);
			ImageIDMap.resize(Num);

			for(int i = 0; i < Num; i++)
			{
				CMapItemImage *pItem = (CMapItemImage *)pDataFile->GetItem(Start + i, 0, 0);
				char *pName = (char *)pDataFile->GetData(pItem->m_ImageName);

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
					// save memory
					pDataFile->UnloadData(pItem->m_ImageName);
					pDataFile->UnloadData(pItem->m_ImageData);
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
					Image.m_pData = pDataFile->GetData(pItem->m_ImageData);
				Image.m_Index = NumImages++;

				MapImages.push_back(Image);
			}
		}

		// load groups
		int LayersStart, LayersNum;
		pDataFile->GetType(MAPITEMTYPE_LAYER, &LayersStart, &LayersNum);

		int Start, Num;
		pDataFile->GetType(MAPITEMTYPE_GROUP, &Start, &Num);

		for(int g = 0; g < Num; g++)
		{
			CMapItemGroup *pGItem = (CMapItemGroup *)pDataFile->GetItem(Start + g, 0, 0);
			bool IsGameGroup = false;

			if(pGItem->m_Version < 1 || pGItem->m_Version > CMapItemGroup::CURRENT_VERSION)
				continue;

			SLayerGroup LayerGroup;
			LayerGroup.m_ParallaxX = pGItem->m_ParallaxX;
			LayerGroup.m_ParallaxY = pGItem->m_ParallaxY;
			LayerGroup.m_OffsetX = pGItem->m_OffsetX;
			LayerGroup.m_OffsetY = pGItem->m_OffsetY;

			if(pGItem->m_Version >= 2)
			{
				LayerGroup.m_UseClipping = pGItem->m_UseClipping;
				LayerGroup.m_ClipX = pGItem->m_ClipX;
				LayerGroup.m_ClipY = pGItem->m_ClipY;
				LayerGroup.m_ClipW = pGItem->m_ClipW;
				LayerGroup.m_ClipH = pGItem->m_ClipH;
			}
			else
			{
				LayerGroup.m_UseClipping = true;
				LayerGroup.m_ClipX = 0;
				LayerGroup.m_ClipY = 0;
				LayerGroup.m_ClipW = 0;
				LayerGroup.m_ClipH = 0;
			}

			if(pGItem->m_Version >= 3)
			{
				LayerGroup.m_aName[0] = pGItem->m_aName[0];
				LayerGroup.m_aName[1] = pGItem->m_aName[1];
				LayerGroup.m_aName[2] = pGItem->m_aName[2];
			}
			else
			{
				StrToInts(LayerGroup.m_aName, sizeof(LayerGroup.m_aName) / sizeof(int), "Group");
			}

			for(int l = 0; l < pGItem->m_NumLayers; l++)
			{
				CMapItemLayer *pLayerItem = (CMapItemLayer *)pDataFile->GetItem(LayersStart + pGItem->m_StartLayer + l, 0, 0);
				if(!pLayerItem)
					continue;

				if(pLayerItem->m_Type == LAYERTYPE_TILES)
				{
					CMapItemLayerTilemap *pTilemapItem = (CMapItemLayerTilemap *)pLayerItem;
					if(pTilemapItem->m_Flags)
						IsGameGroup = true;

					CMapItemLayerTilemap Item;
					Item.m_Version = 3;
					Item.m_Layer.m_Version = 0;
					Item.m_Layer.m_Flags = pTilemapItem->m_Layer.m_Flags;
					Item.m_Layer.m_Type = pTilemapItem->m_Layer.m_Type;

					Item.m_Color = pTilemapItem->m_Color;
					Item.m_ColorEnv = -1;
					Item.m_ColorEnvOffset = 0;

					Item.m_Width = pTilemapItem->m_Width;
					Item.m_Height = pTilemapItem->m_Height;
					Item.m_Flags = pTilemapItem->m_Flags & TILESLAYERFLAG_GAME;

					Item.m_Image = pTilemapItem->m_Image >= 0 ? ImageIDMap[pTilemapItem->m_Image] : -1;

					Item.m_Tele = -1;
					Item.m_Speedup = -1;
					Item.m_Front = -1;
					Item.m_Switch = -1;
					Item.m_Tune = -1;

					void *pData = pDataFile->GetData(pTilemapItem->m_Data);
					// unsigned int Size = pDataFile->GetDataSize(pTilemapItem->m_Data);

					if(pTilemapItem->m_Version >= 3)
					{
						Item.m_aName[0] = pTilemapItem->m_aName[0];
						Item.m_aName[1] = pTilemapItem->m_aName[1];
						Item.m_aName[2] = pTilemapItem->m_aName[2];
					}
					else
					{
						StrToInts(Item.m_aName, sizeof(Item.m_aName) / sizeof(int), "Layer");
					}

					Item.m_Data = DataFileOut.AddData((size_t)Item.m_Width * Item.m_Height * sizeof(CTile), pData);
					DataFileOut.AddItem(MAPITEMTYPE_LAYER, l, sizeof(Item), &Item);
				}
			}

			CMapItemGroup GItem;
			GItem.m_Version = CMapItemGroup::CURRENT_VERSION;

			GItem.m_ParallaxX = LayerGroup.m_ParallaxX;
			GItem.m_ParallaxY = LayerGroup.m_ParallaxY;
			GItem.m_OffsetX = LayerGroup.m_OffsetX;
			GItem.m_OffsetY = LayerGroup.m_OffsetY;
			GItem.m_UseClipping = LayerGroup.m_UseClipping;
			GItem.m_ClipX = LayerGroup.m_ClipX;
			GItem.m_ClipY = LayerGroup.m_ClipY;
			GItem.m_ClipW = LayerGroup.m_ClipW;
			GItem.m_ClipH = LayerGroup.m_ClipH;
			GItem.m_aName[0] = LayerGroup.m_aName[0];
			GItem.m_aName[1] = LayerGroup.m_aName[1];
			GItem.m_aName[2] = LayerGroup.m_aName[2];
			GItem.m_StartLayer = pGItem->m_StartLayer;
			GItem.m_NumLayers = pGItem->m_NumLayers;
			DataFileOut.AddItem(MAPITEMTYPE_GROUP, g, sizeof(GItem), &GItem);
		}

		DataFiles.push_back(pDataFile);

		break;
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
				Item.m_ImageData = DataFileOut.AddData(Item.m_Width * Item.m_Height * 4, Image.m_pData);
			DataFileOut.AddItem(MAPITEMTYPE_IMAGE, Image.m_Index, sizeof(Item), &Item);
		}
	}
	DataFileOut.Finish();

	// free data
	for(auto &pDataFile : DataFiles)
	{
		pDataFile->Close();
		delete pDataFile;
	}

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

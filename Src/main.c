#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <windows.h>

#include "./LinkList/LinkedList.h"

#define STRING(str) #str
#define ERROR_INFO(info) STRING(\x1b[41m[ERROR]\x1b[0m \x1b[31m ##info\x1b[0m)

char _line_text[512];
char entry_point[11] = {0};

typedef struct {
	LListNode_t node;
	char type;
	char name[30];
	int base;
	int size;
	int max;
} Region_t;

typedef struct {
	LListNode_t node;
	LListRoot_t load_region;
} LoadRegion_t;

typedef struct {
	LListNode_t node;
	int rom_size;
	int ram_size;
	char name[];
} object_t;

typedef struct {
	int entry_point;
	LListRoot_t load_regions;
	LListRoot_t objects;
	struct {
		int rom_size;
		int ram_size;
	} library_totals;
} MapInfo_t;
MapInfo_t ThisMapInfo = {.load_regions = LLIST_STATIC_ROOTINIT(ThisMapInfo.load_regions), .objects = LLIST_STATIC_ROOTINIT(ThisMapInfo.objects)};
MapInfo_t LastMapInfo = {.load_regions = LLIST_STATIC_ROOTINIT(LastMapInfo.load_regions), .objects = LLIST_STATIC_ROOTINIT(LastMapInfo.objects)};

void GetRegionInfo(LListRoot_t* root)
{
	Region_t* region = (Region_t*)malloc(sizeof(Region_t));
	LList_PointToSelf(&(region->node));
	memset(region, 0, sizeof(Region_t));
	if (strstr(_line_text, "Load Region") != NULL) {
		region->type = 'L';
	} else {
		region->type = 'E';
	}
	char* temp_p = strstr(_line_text, "Region ");
	memcpy_s(region->name, sizeof(region->name), temp_p + 7, strstr(_line_text, " (") - strstr(_line_text, "Region ") - 7);
	char temp_str[11] = {0};
	memcpy_s(temp_str, sizeof(temp_str), strstr(_line_text, "Base: ") + 6, 10);
	region->base = strtoul(temp_str, NULL, 16);
	memcpy_s(temp_str, sizeof(temp_str), strstr(_line_text, "Size: ") + 6, 10);
	region->size = strtoul(temp_str, NULL, 16);
	memcpy_s(temp_str, sizeof(temp_str), strstr(_line_text, "Max: ") + 5, 10);
	region->max = strtoul(temp_str, NULL, 16);
	LList_AddToTail(root, &(region->node));
}

void SearchLoadRegion(FILE* mapfile, LListRoot_t* load_regions)
{
	LoadRegion_t* loadregion = NULL;
	while (strstr(_line_text, "=======") == NULL) {
		if (strstr(_line_text, "Region ") != NULL) {
			if (strstr(_line_text, "Load") != NULL) {
				loadregion = (LoadRegion_t*)malloc(sizeof(LoadRegion_t));
				LList_PointToSelf(&(loadregion->node));
				LList_PointToSelf(&(loadregion->load_region));
				LList_AddToTail(load_regions, (LListNode_t*)loadregion);
			}
			GetRegionInfo(&(loadregion->load_region));
		}
		fgets(_line_text, sizeof(_line_text), mapfile);
	}
}

int cmpfunc(const void* a, const void* b)
{
	return strcmp(strlwr((*(object_t**)a)->name), strlwr((*(object_t**)b)->name));
}

void print_progress(int current, int total, int stacksize, int heapsize)
{
	const int bar_width = 75;
	float progress		= (float)current / total;
	int stackpos		= ((float)stacksize) / total * bar_width;
	int heappos			= ((float)heapsize) / total * bar_width;
	int pos				= progress * bar_width;

	printf("[");
	for (int i = 0; i < bar_width; ++i) {
		if (i < pos - stackpos - heappos) {
			printf("■");
		} else if (i < pos - heappos) {
			printf("o");
		} else if (i < pos) {
			printf("□");
		} else {
			printf(" ");
		}
	}
	printf("]%6.2f%%", progress * 100);
}

int main(int argc, char* argv[])
{
	int retval		 = 0;
	char* _input_dir = NULL;

#pragma region 解析命令行参数
	int ch;
	while ((ch = getopt(argc, argv, "vhp:")) != -1) {
		switch (ch) {
			case 'v':
				printf("测试版本，版本号未知\n");
				return 0;

			case 'h':
				printf("帮助\n");
				return 0;

			case 'p':
				_input_dir = optarg;
				break;

			default:
				printf(ERROR_INFO(非法的输入参数));
				return -1;
		}
	}
#pragma endregion

#pragma region 目录解析
	char* mapfile_dir = NULL;
	int buff_len	  = 0;
	if (strstr(_input_dir, ":\\") == NULL && strstr(_input_dir, ":/") == NULL) { // 没有盘符
		buff_len = GetCurrentDirectory(0, NULL);
		if (buff_len == 0) {
			printf(ERROR_INFO(获取当前目录长度错误出错！));
			retval = 1;
			goto __exit;
		}
		mapfile_dir = (char*)malloc(buff_len + strlen(_input_dir) + 2);
		if (mapfile_dir == NULL) {
			printf(ERROR_INFO(申请内存失败！));
			retval = 2;
			goto __exit;
		}
		buff_len = GetCurrentDirectory(buff_len, mapfile_dir);
		if (buff_len == 0) {
			printf(ERROR_INFO(获取当前目录错误出错！));
			retval = 3;
			goto __exit;
		}

		strcat(mapfile_dir, "\\");
		strcat(mapfile_dir, _input_dir);
	} else { // 有盘符
		mapfile_dir = _input_dir;
	}
	DWORD attributes = GetFileAttributes(mapfile_dir);
	if (attributes == INVALID_FILE_ATTRIBUTES || strstr(mapfile_dir, ".map") == NULL) {
		printf(ERROR_INFO(找不到合法文件，请检查.map文件路径));
		retval = 4;
		goto __exit;
	}
#pragma endregion

#pragma region 映射文件(.map)解析
	/* 打开并读取 map 文件 */
	FILE* mapfile = fopen(mapfile_dir, "r");
	if (mapfile == NULL) {
		retval = 4;
		printf(ERROR_INFO(map 文件无法打开));
		goto __exit;
	}
	fseek(mapfile, 0, SEEK_END);	// 设定流的位置为文件末尾
	long filesize = ftell(mapfile); // 获取文件大小
	fseek(mapfile, 0, SEEK_SET);

	/* 解析 Image Symbol Table */
	char* p_temp = 0;
	while (ftell(mapfile) < filesize) {
		fgets(_line_text, sizeof(_line_text), mapfile);
		if (strstr(_line_text, "Image Symbol Table") != NULL) {
			break;
		}
	}

	long temp			= ftell(mapfile);
	long stack_location = 0;
	long heap_location	= 0;
	long stack_size		= 0;
	long heap_size		= 0;
	while (ftell(mapfile) < filesize) {
		fgets(_line_text, sizeof(_line_text), mapfile);
		if (strstr(_line_text, "Heap_Mem") != NULL) {
			heap_location = strtoul(_line_text + 41, NULL, 16);
			heap_size	  = strtoul(_line_text + 69, NULL, 10);
		} else if (strstr(_line_text, "Stack_Mem") != NULL) {
			stack_location = strtoul(_line_text + 41, NULL, 16);
			stack_size	   = strtoul(_line_text + 69, NULL, 10);
		} else if (strstr(_line_text, "Global Symbols") != NULL) {
			break;
		}
	}

	/* 解析 Memory Map of the image */
	while (ftell(mapfile) < filesize) {
		fgets(_line_text, sizeof(_line_text), mapfile);
		p_temp = strstr(_line_text, "Image Entry point : ");
		if (p_temp != NULL) {
			ThisMapInfo.entry_point = strtol(p_temp + 20, NULL, 16);
			SearchLoadRegion(mapfile, &(ThisMapInfo.load_regions));
			break;
		}
	}

	/* 解析 Image component sizes */
	/* 解析用户文件 */
	char* token		 = NULL;
	int rom_size	 = 0;
	int ram_size	 = 0;
	int name_len	 = 0;
	int object_count = 0;
	while (strstr(_line_text, "Code (inc. data)") == NULL) {
		fgets(_line_text, sizeof(_line_text), mapfile);
	}
	temp = ftell(mapfile);
	while (ftell(mapfile) < filesize) {
		fgets(_line_text, sizeof(_line_text), mapfile);
		if (strstr(_line_text, ".o") != NULL) {
			object_count++;
		} else if (strstr(_line_text, "---------------------------------") != NULL) {
			break;
		}
	}
	fseek(mapfile, temp, SEEK_SET);
	object_t** objects_array = (object_t**)malloc(sizeof(object_t*) * object_count);
	object_count			 = 0;

	while (ftell(mapfile) < filesize) {
		fgets(_line_text, sizeof(_line_text), mapfile);
		if (strstr(_line_text, ".o") != NULL) {
			token	 = strtok(_line_text, " "); // Code
			rom_size = strtoul(token, NULL, 10);
			token	 = strtok(NULL, " "); // (inc. data)

			token = strtok(NULL, " "); // RO Data
			rom_size += strtoul(token, NULL, 10);

			token = strtok(NULL, " "); // RW Data
			rom_size += strtoul(token, NULL, 10);
			ram_size = strtoul(token, NULL, 10);

			token = strtok(NULL, " "); // ZI Data
			ram_size += strtoul(token, NULL, 10);

			token = strtok(NULL, " "); // Debug

			token	 = strtok(NULL, " "); // name
			name_len = strrchr(token, 'o') - token;

			objects_array[object_count] = (object_t*)malloc(sizeof(object_t) + name_len + 2);
			LList_PointToSelf(&(objects_array[object_count]->node));
			memcpy(objects_array[object_count]->name, token, name_len);
			objects_array[object_count]->name[name_len]		= 'c';
			objects_array[object_count]->name[name_len + 1] = '\0';
			objects_array[object_count]->rom_size			= rom_size;
			objects_array[object_count]->ram_size			= ram_size;
			object_count++;
		} else if (strstr(_line_text, "---------------------------------") != NULL) {
			break;
		}
	}
	qsort(objects_array, object_count - 1, sizeof(object_t*), cmpfunc);
	for (int i = 0; i < object_count; i++) {
		LList_AddToTail(&(ThisMapInfo.objects), &(objects_array[object_count - 1 - i]->node));
	}
	/* 解析库文件 */
	fseek(mapfile, 400, SEEK_CUR); // 跳过 400 字节
	while (ftell(mapfile) < filesize) {
		fgets(_line_text, sizeof(_line_text), mapfile);
		if (strstr(_line_text, "Library Totals") != NULL || strstr(_line_text, "incl. Padding") != NULL) {
			token								= strtok(_line_text, " "); // Code
			ThisMapInfo.library_totals.rom_size = strtoul(token, NULL, 10);
			token								= strtok(NULL, " "); // (inc. data)

			token = strtok(NULL, " "); // RO Data
			ThisMapInfo.library_totals.rom_size += strtoul(token, NULL, 10);

			token = strtok(NULL, " "); // RW Data
			ThisMapInfo.library_totals.rom_size += strtoul(token, NULL, 10);
			ThisMapInfo.library_totals.ram_size = strtoul(token, NULL, 10);

			token = strtok(NULL, " "); // ZI Data
			ThisMapInfo.library_totals.ram_size += strtoul(token, NULL, 10);
		}
	}
	fclose(mapfile);
#pragma endregion

#pragma region 读取调用最大堆栈
	/* 打开并读取 htm 文件 */
	memcpy(strrchr(mapfile_dir, '.'), ".htm", sizeof(".htm"));
	mapfile = fopen(mapfile_dir, "r");
	if (mapfile == NULL) {
		retval = 1;
		printf(ERROR_INFO(htm 文件无法打开));
		goto __exit;
	}
	fseek(mapfile, 0, SEEK_END); // 设定流的位置为文件末尾
	filesize = ftell(mapfile);	 // 获取文件大小
	fseek(mapfile, 0, SEEK_SET);

	int maxstack_usage = 0;
	while (ftell(mapfile) < filesize) {
		fgets(_line_text, sizeof(_line_text), mapfile);
		if (strstr(_line_text, "Maximum Stack Usage =") != NULL) {
			token		   = strtok(_line_text + 28, " ");
			maxstack_usage = strtoul(token, NULL, 10);
		}
	}
	fclose(mapfile);
#pragma endregion

#pragma region 保存&读取
	LListNode_t* this		 = NULL;
	LListNode_t* this_region = NULL;
	/* 读取上次记录 */
	memcpy(strrchr(mapfile_dir, '.'), ".db", sizeof(".db"));
	mapfile = fopen(mapfile_dir, "r");
	if (mapfile != NULL) {
		fseek(mapfile, 0, SEEK_END); // 设定流的位置为文件末尾
		filesize = ftell(mapfile);	 // 获取文件大小
		fseek(mapfile, 196, SEEK_SET);

		object_t* object;
		while (ftell(mapfile) < filesize) {
			fgets(_line_text, sizeof(_line_text), mapfile);
			if (strstr(_line_text, "==============") != NULL) {
				break;
			}
			token  = strtok(_line_text, " ");
			object = (object_t*)malloc(sizeof(object_t) + strlen(token) + 1);
			LList_PointToSelf(&(object->node));
			memcpy(object->name, token, strlen(token));
			object->name[strlen(token)] = '\0';
			object->rom_size			= strtoul(strtok(NULL, " "), NULL, 10);
			object->ram_size			= strtoul(strtok(NULL, " "), NULL, 10);
			LList_AddToTail(&(LastMapInfo.objects), &(object->node));
		}
		fgets(_line_text, sizeof(_line_text), mapfile);
		SearchLoadRegion(mapfile, &(LastMapInfo.load_regions));

		fclose(mapfile);
	}

	/* 保存本次记录 */
	mapfile = fopen(mapfile_dir, "w");
	if (mapfile == NULL) {
		retval = 1;
		goto __exit;
	}
	fprintf(mapfile, "---------------------------------------------------------------\n");
	fprintf(mapfile, "           File Name                   ROM            RAM      \n");
	fprintf(mapfile, "---------------------------------------------------------------\n");
	LList_ForEach(&(ThisMapInfo.objects), this, {
		fprintf(mapfile, "    %-25s        %-7d        %-7d      \n", ((object_t*)this)->name, ((object_t*)this)->rom_size, ((object_t*)this)->ram_size);
	});
	fprintf(mapfile, "===============================================================\n");
	LList_ForEach(&(ThisMapInfo.load_regions), this, {
		LListRoot_t* this_load_region = &(((LoadRegion_t*)this)->load_region);
		LList_ForEach(this_load_region, this_region, {
			if (((Region_t*)this_region)->type == 'L') {
				fprintf(mapfile, "Load ");
			} else {
				fprintf(mapfile, "Execution ");
			}
			fprintf(mapfile, "Region %s (Base: 0x%08x, Size: 0x%08x, Max: 0x%08x)\n", ((Region_t*)this_region)->name, ((Region_t*)this_region)->base, ((Region_t*)this_region)->size, ((Region_t*)this_region)->max);
		});
	});

	fprintf(mapfile, "===============================================================\n");
	fclose(mapfile);
#pragma endregion

#pragma region 输出数据
	int count = 0;
	int a	  = 0;
	int b	  = 0;
	char temp0[10];
	char temp1[10];
	char temp2[10];
	printf("________________________________________________________________________________________\n");
	printf("|           File Name                |          ROM           |          RAM           |\n");
	printf("|------------------------------------|------------------------|------------------------|\n");
	LListNode_t* _this = LastMapInfo.objects.next;

	LList_ForEach(&(ThisMapInfo.objects), this, {
		sprintf(temp0, "[new]");
		temp1[0] = '\0';
		temp2[0] = '\0';
		LList_ForEach(&(LastMapInfo.objects), _this, {
			if (!strcmp(((object_t*)this)->name, ((object_t*)_this)->name)) {
				temp0[0] = '\0';
				if (((object_t*)this)->ram_size - ((object_t*)_this)->ram_size != 0) {
					sprintf(temp2, "[%+d]", (int)((object_t*)this)->ram_size - ((object_t*)_this)->ram_size);
				} else {
					temp2[0] = '\0';
				}
				if (((object_t*)this)->rom_size - ((object_t*)_this)->rom_size != 0) {
					sprintf(temp1, "[%+d]", (int)((object_t*)this)->rom_size - ((object_t*)_this)->rom_size);
				} else {
					temp1[0] = '\0';
				}
				break;
			}
		});
		printf("|    %25s %-6s|   %10d %-10s|   %10d %-10s|  \n", ((object_t*)this)->name, temp0, ((object_t*)this)->rom_size, temp1, ((object_t*)this)->ram_size, temp2);
	});
	printf("‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾\n");

	LList_ForEach(&(ThisMapInfo.load_regions), this, {
		count						   = 0;
		LListRoot_t* _this_load_region = NULL;
		LListRoot_t* this_load_region  = &(((LoadRegion_t*)this)->load_region);

		LList_ForEach(this_load_region, this_region, {
			if (count == 0) { // 加载域
				printf("%s :\n", ((Region_t*)this_region)->name);
				LList_ForEach(&(LastMapInfo.load_regions), _this, {
					if (!strcmp(((Region_t*)(((LoadRegion_t*)_this)->load_region).next)->name, ((Region_t*)this_region)->name)) {
						_this_load_region = &(((LoadRegion_t*)_this)->load_region);
					}
				});
			} else {
				LList_ForEach(_this_load_region, _this, {
					if (!strcmp(((Region_t*)_this)->name, ((Region_t*)this_region)->name)) {
						a = ((Region_t*)this_region)->size - ((Region_t*)_this)->size;
						if (a != 0) {
							sprintf(temp0, "[%+d]", a);
						} else {
							temp0[0] = '\0';
						}
					}
				});
				if (((Region_t*)this_region)->base <= stack_location && stack_location <= ((Region_t*)this_region)->base + ((Region_t*)this_region)->max) {
					a = stack_size;
				} else {
					a = 0;
				}
				if (((Region_t*)this_region)->base <= heap_location && heap_location <= ((Region_t*)this_region)->base + ((Region_t*)this_region)->max) {
					b = heap_size;
				} else {
					b = 0;
				}

				printf(" %10s ", ((Region_t*)this_region)->name);
				print_progress(((Region_t*)this_region)->size, ((Region_t*)this_region)->max, a, b);
				printf(" (%6.02fKb/%.02fKb in 0x%08x) %s\n", ((Region_t*)this_region)->size / 1024.0f, ((Region_t*)this_region)->max / 1024.0f, ((Region_t*)this_region)->base, temp0);
			}
			count++;
		});
	});
	printf("Entry point: 0x%08x		Static Maximum Stack Usage: %dbytes(%.2f%%)\n", ThisMapInfo.entry_point, maxstack_usage, (maxstack_usage * 100.0f) / stack_size);
#pragma endregion

#pragma region 清理
__exit:
	switch (retval) {
		case 0:
		case 1:
			LListNode_t* this		 = NULL;
			LListNode_t* this_region = NULL;
			LList_ForEach(&(ThisMapInfo.load_regions), this, {
				LList_DeleteAll((LListRoot_t*)(&(((LoadRegion_t*)this)->load_region)));
				this = LList_Delete(this);
			});
			LList_DeleteAll(&(ThisMapInfo.objects));
			LList_DeleteAll(&(LastMapInfo.objects));
		case 4:
		case 3:
		case 2:
			free(mapfile_dir);
		default:
			return retval;
	}
#pragma endregion
}

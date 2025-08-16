/*****************************************************************************************************
 * @file        LinkedList.h
 * @author      RegLucis
 * @version     V0.9
 * @date        2024/06/06
 * @brief       提供链表操作支持
 * @email		Reglucis@outlook.com
 ****************************************************************************************************
 * @note
 * 				使用双向链表实现，约定所有链表均为环状结构
 *
 * History Ver:
 * 			V0.1		2023/12/16		Reglucis		创建文件
 *			V0.9		2024/06/06		Reglucis		更新为双向链表
 *
 * @todo
 *
 *
 ****************************************************************************************************
 * @test
 *
 *
 * @bug
 *
 *
 ****************************************************************************************************
 * @example
 *
 *
 ***************************************************************************************************/

#ifndef __LINKED_LIST__
#define __LINKED_LIST__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "./Sys/Compiler/Compiler.h"
#pragma region 数据结构

typedef struct __LinkedListNode {
	struct __LinkedListNode* prev;
	struct __LinkedListNode* next;
} LListNode_t;

typedef struct __LinkedListNode LListRoot_t; // 链表根节点，使用前必须初始化（自指）

#pragma endregion

#pragma region 函数声明

void* LList_New(uint32_t item_size);
void LList_Insert(LListNode_t* prevNode, LListNode_t* this);
void* LList_Delete(LListNode_t* this);
static inline void LList_PointToSelf(LListNode_t* this);
static inline LListNode_t* LList_Remove(LListNode_t* this);
static inline LListNode_t* LList_GetTailNode(LListRoot_t* root);
static inline void LList_AddToTail(LListRoot_t* root, LListNode_t* this);
static inline void LList_Free(LListNode_t* this);

#pragma endregion

#pragma region 宏函数/内联函数

/********************************************************************
 * @brief		根节点静态初始化
 * @param		root: 根变量名
 * @example		LListRoot_t* root = LLIST_STATIC_ROOTINIT(root);
 ********************************************************************/
#define LLIST_STATIC_ROOTINIT(root) {&(root), &(root)}

/********************************************************************
 * @brief		使结点自指
 * @param		this: 被操作结点
 ********************************************************************/
static inline void LList_PointToSelf(LListNode_t* this)
{
	this->prev = this;
	this->next = this;
}

/********************************************************************
 * @brief		判断结点是否孤立
 * @param		this: 被操作结点
 * @return		bool值: 孤立为 true
 ********************************************************************/
static bool SLList_IsIsolated(LListNode_t* this)
{
	return (this->next == this);
}

/********************************************************************
 * @brief		返回尾节点
 * @param		this: 被操作结点
 * @return		bool值: 孤立为 true
 ********************************************************************/
static inline LListNode_t* LList_GetTailNode(LListRoot_t* root)
{
	return root->prev;
}

/********************************************************************
 * @brief		在尾部添加结点
 * @param		root: 根结点
 * @param		this: 被插入结点
 ********************************************************************/
static inline void LList_AddToTail(LListRoot_t* root, LListNode_t* this)
{
	LList_Insert(LList_GetTailNode(root), this);
}

/********************************************************************
 * @brief		释放一个结点
 * @warning		不允许删除任何非独立结构
 * @attention	必须先使用 LList_Remove 从当前结构移除结点再释放
 ********************************************************************/
static inline void LList_Free(LListNode_t* this)
{
	free(this);
}

/********************************************************************
 * @brief		遍历环中全部结点
 * @param		root: 进入结点
 * @param		this: 遍历过程中的子结点
 * @param		...: 伪 Lambda 表达式, 遍历过程中的操作
 * @attention	宏扩展函数, 注意参数合法性
 * @attention	注意根节点不包含数据，因此遍历时必须从根节点开始，且只能有一个根节点
 * @example
 * 			LListRoot_t root;	(global var)
 * 				...
 * 			LListNode_t* this;
 * 			LList_foreach(&root, this, {
 *				int a = 0;
 *				a++;
 *			});
 ********************************************************************/
#define LList_ForEach(root, this, ...)                            \
	if ((root) != NULL) {                                         \
		(this) = (void*)(root);                                   \
		while ((LListNode_t*)(this) != LList_GetTailNode(root)) { \
			(this) = (void*)(((LListNode_t*)(this))->next);       \
			{__VA_ARGS__};                                        \
		}                                                         \
	}

/********************************************************************
 * @brief		释放整个链表
 ********************************************************************/
extern int malloc_count;
static inline void LList_DeleteAll(LListRoot_t* root)
{
	LListNode_t* this = NULL;
	LList_ForEach(root, this, {
		this = LList_Delete(this);
		malloc_count--;
	});
}

#pragma endregion
#endif

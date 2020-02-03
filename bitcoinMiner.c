#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <stdbool.h>
#include <zlib.h>

#define LEN 500
#define NUM_MINERS 4
#define BITS_IN_UINT 32
#define DIFFICULTY_VAL 16

#define assert_if(errnum) if (errnum != 0) {printf("Error: %m\n"); exit(EXIT_FAILURE);}

#define POLICY_STR(policy)  (policy == SCHED_FIFO)  ? "SCHED_FIFO" :	\
		                  (policy == SCHED_RR)    ? "SCHED_RR" :	\
                   		  (policy == SCHED_OTHER) ? "SCHED_OTHER" :	\
		                  "???"

typedef struct {
	int height;              // Incrementeal ID of the block in the chain
	int timestamp;           // Time of the mine in seconds since epoch
	unsigned int hash;       // Current block hash value
	unsigned int prev_hash;  // Hash value of the previous block
	int difficulty;          // Amount of preceding zeros in the hash
	int nonce;               // Incremental integer to change the hash value
	int relayed_by;          // Miner ID
} BLOCK_T;


typedef struct ListNode {
	BLOCK_T block;
	struct ListNode *next;
} ListNode;

typedef struct List {
	struct ListNode *head;
} List;


/* Function Declarations ~~ START */
pthread_attr_t setServerThreadPriority();
void addBlockToListHead(BLOCK_T newBlock);
void removeBlockFromListHead(ListNode* nodeToRemove);
void createGenesisBlock();
BLOCK_T server_CreateNewBlock(BLOCK_T prev_block);
void miner_UpdateBlock(BLOCK_T* blockToUpdate, int miner_id);
char* convertBlockToString(BLOCK_T block);
char* convertDecimalToBinary(unsigned int num);
bool isValidHash(unsigned int crc32res);
void* Server();
void* Miner(void* minerId);
void* BadMiner(void* minerId);
/* Function Declarations ~~ END */


/* Global Variables ~~ START */
int g_height = 0;
List g_blockchain;
BLOCK_T g_newlyMinedBlock;
BLOCK_T g_currentBlockchainHead;

pthread_mutex_t g_server_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t g_miner_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_mined_new_block = PTHREAD_COND_INITIALIZER;
/* Global Variables ~~ END */


void main()
{
	int j, res = -1;
	int miner_threads_id[NUM_MINERS + 1] = { 1, 2, 3, 4, 5 };
	pthread_attr_t server_attr;
	pthread_t server, badMiner;
	pthread_t miners[NUM_MINERS];
	ListNode* nextNode;

	createGenesisBlock();

	server_attr = setServerThreadPriority();
	res = pthread_create(&server, &server_attr, Server, NULL);
	assert_if(res);


	for (j = 0; j < NUM_MINERS; j++)
	{
		pthread_create(&miners[j], NULL, Miner, &miner_threads_id[j]);
	}

	pthread_create(&badMiner, NULL, BadMiner, &miner_threads_id[NUM_MINERS]);


	pthread_join(server, NULL);

	for (j = 0; j < NUM_MINERS; j++)
	{
		pthread_join(miners[j], NULL);
	}

	pthread_join(badMiner, NULL);

	// free memory allocations
	while (g_blockchain.head != NULL)
	{
		nextNode = g_blockchain.head->next;

		free(g_blockchain.head);
		g_blockchain.head = nextNode;
	}
}


pthread_attr_t setServerThreadPriority()
{
	int res = -1, priority = 80;
	pthread_attr_t server_attr;
	struct sched_param server_priority;


	res = pthread_attr_init(&server_attr);
	assert_if(res);

	res = pthread_attr_getschedparam(&server_attr, &server_priority);
	assert_if(res);
	server_priority.sched_priority = priority;

	 res = pthread_attr_setschedpolicy(&server_attr, SCHED_FIFO);
	assert_if(res);

	res = pthread_attr_setschedparam(&server_attr, &server_priority);
	assert_if(res);

	res = pthread_attr_setinheritsched(&server_attr, PTHREAD_EXPLICIT_SCHED);
	assert_if(res);

	return server_attr;
}

void addBlockToListHead(BLOCK_T newBlock)
{
	ListNode* newNode = (ListNode*)calloc(1, sizeof(ListNode));
	newNode->block = newBlock;
	newNode->next = g_blockchain.head;
	g_blockchain.head = newNode;
}

void removeBlockFromListHead(ListNode* nodeToRemove)
{
	if (g_blockchain.head != NULL)
	{
		g_blockchain.head = nodeToRemove->next;
	}

	free(nodeToRemove);
}

void createGenesisBlock()
{
	BLOCK_T new_block;

	new_block.height = g_height;
	new_block.prev_hash = 0;
	new_block.difficulty = DIFFICULTY_VAL;

	g_blockchain.head = (ListNode*)calloc(1, sizeof(ListNode));
	g_blockchain.head->block = new_block;
	g_blockchain.head->next = NULL;
	g_currentBlockchainHead = g_blockchain.head->block;
}

BLOCK_T server_CreateNewBlock(BLOCK_T prev_block)
{
	BLOCK_T block;

	g_height++;
	block.height = g_height;
	block.difficulty = DIFFICULTY_VAL;
	block.prev_hash = prev_block.hash;

	return block;
}

void miner_UpdateBlock(BLOCK_T* blockToUpdate, int miner_id)
{
	time_t curTime;
	int currentTime;  // Time of the mine in seconds since epoch

	curTime = time(NULL);
	currentTime = (int)curTime;

	(*blockToUpdate).nonce = 0;
	(*blockToUpdate).relayed_by = miner_id;
	(*blockToUpdate).timestamp = currentTime;
}

char* convertBlockToString(BLOCK_T block)
{
	char* blockStr = (calloc(LEN, sizeof(char)));
	snprintf(blockStr, LEN, "%d%d%u%d%d", block.height, block.timestamp, block.prev_hash, block.nonce, block.relayed_by);

	return blockStr;
}

bool isValidHash(unsigned int crc32res)
{
	//valid hashes for difficulty 16: between 0 and 65535 (2^16 - 1)
	double highestValidHash = pow(2, BITS_IN_UINT - DIFFICULTY_VAL) - 1;

	if (crc32res <= highestValidHash)
	{
		return true;
	}

	return false;
}

bool isBlockValid(BLOCK_T blockToCheck)
{
	bool isValid = true;
	bool isInvalidTimestamp;
	bool isInvalidHeight = (blockToCheck.height != g_blockchain.head->block.height);
	bool isInvalidPrevHash = (blockToCheck.prev_hash != g_blockchain.head->block.prev_hash);

	if (isInvalidHeight)
	{
		isValid = false;
		printf("--> Server: Block height out of order for block #%d by miner %d, received %d but should be %d\n", g_newlyMinedBlock.height, g_newlyMinedBlock.relayed_by, g_newlyMinedBlock.height, g_blockchain.head->block.height);
	}
	else if (isInvalidPrevHash)
	{
		isValid = false;
		printf("--> Server: Wrong prev_hash for block #%d by miner %d, received 0x%x but should be 0x%x\n", blockToCheck.height, blockToCheck.relayed_by, blockToCheck.prev_hash, g_blockchain.head->block.prev_hash);
	}
	else if (g_blockchain.head->next != NULL)
	{
		isInvalidTimestamp = blockToCheck.timestamp < g_blockchain.head->next->block.timestamp;

		if (isInvalidTimestamp)
		{
			isValid = false;
			printf("--> Server: Block timestamp is prior to the previous blocks in the blockchain\n");
		}
	}


	return isValid;
}

void* Server()
{
	unsigned int crc32res;
	char* blockStr;
	bool isNewBlockValid;
	BLOCK_T newHeadBlock;


	for (;;)
	{
		pthread_mutex_lock(&g_server_lock);

		// wait for a signal from one of the miners about a successfully mined block
		pthread_cond_wait(&g_mined_new_block, &g_server_lock);

		// "newlyMinedBlock" is the new block that the miner just mined
		blockStr = convertBlockToString(g_newlyMinedBlock);
		crc32res = crc32(0, blockStr, strlen(blockStr));
		free(blockStr);

		if (crc32res == g_newlyMinedBlock.hash && isValidHash(crc32res))  // the new block hash is valid
		{
			isNewBlockValid = isBlockValid(g_newlyMinedBlock);

			if (isNewBlockValid)  // if the block is valid - update blockchain
			{
				removeBlockFromListHead(g_blockchain.head);  // to avoid duplications in the blockchain
				addBlockToListHead(g_newlyMinedBlock);
				printf("Server: New block added by %d, attributes: height(%d), timestamp(%d), hash(0x%x), prev_hash(0x%x), difficulty(%d), nonce(%d)\n\n", g_newlyMinedBlock.relayed_by, g_newlyMinedBlock.height, g_newlyMinedBlock.timestamp, g_newlyMinedBlock.hash, g_newlyMinedBlock.prev_hash, g_newlyMinedBlock.difficulty, g_newlyMinedBlock.nonce);
				
				newHeadBlock = server_CreateNewBlock(g_blockchain.head->block);
				addBlockToListHead(newHeadBlock);  // server creates a new half-empty block for miners to complete
				g_currentBlockchainHead = g_blockchain.head->block;

				if (g_blockchain.head->block.height > 100)
				{
					// STOP THE PROGRAM! blockchain has over 100 blocks.
					exit(0);
				}
			}
		}
		else  // the block has invalid hash
		{
			printf("--> Server: Wrong hash for block #%d by miner %d, received 0x%x (which is invalid)\n", g_newlyMinedBlock.height, g_newlyMinedBlock.relayed_by, g_newlyMinedBlock.hash);
		}

		pthread_mutex_unlock(&g_server_lock);
	}
}

void* Miner(void* minerId)
{
	int* minerIdPtr;
	int miner_id;
	int currentNonce = 0;
	char* blockStr;
	BLOCK_T newBlock;
	unsigned int crc32res;

	minerIdPtr = (int*)minerId;
	miner_id = *minerIdPtr;

	for (;;)
	{
		if (newBlock.height == g_height)
		{
			currentNonce = newBlock.nonce + 1;
		}

		newBlock = g_currentBlockchainHead;
		miner_UpdateBlock(&newBlock, miner_id);  // fill all the fields of the block - except hash
		newBlock.nonce = currentNonce;

		blockStr = convertBlockToString(newBlock);
		crc32res = crc32(0, blockStr, strlen(blockStr));
		free(blockStr);

		// run the crc32 function until we get a valid hash
		while (!isValidHash(crc32res))
		{
			// if the current block is irrelevant (someone "beat me") then stop working on this block
			if (newBlock.height < g_currentBlockchainHead.height)  
			{
				break;
			}

			(newBlock.nonce)++;
			newBlock.timestamp = (int)time(NULL);
			blockStr = convertBlockToString(newBlock);
			crc32res = crc32(0, blockStr, strlen(blockStr));
			free(blockStr);
		}
		
		// if the 'while' ended without a valid hash - the current block is irrelevant. get to work on the newest block.
		if (!isValidHash(crc32res))
		{
			continue;
		}

		newBlock.hash = crc32res;

		pthread_mutex_lock(&g_miner_lock);
		g_newlyMinedBlock = newBlock;  // the global variable "g_newlyMinedBlock" contains the new block
		printf("Miner #%d: Mined a new block #%d, with the hash 0x%x\n", miner_id, g_newlyMinedBlock.height, g_newlyMinedBlock.hash);
		pthread_cond_signal(&g_mined_new_block);
		pthread_mutex_unlock(&g_miner_lock);
	}
}

void* BadMiner(void* minerId)
{
	int* minerIdPtr;
	int miner_id;
	BLOCK_T newBlock;

	minerIdPtr = (int*)minerId;
	miner_id = *minerIdPtr;

	for (;;)
	{
		sleep(1);
		newBlock = g_currentBlockchainHead;
		miner_UpdateBlock(&newBlock, miner_id);
		newBlock.hash = 12245933;  // invalid hash

		pthread_mutex_lock(&g_miner_lock);
		g_newlyMinedBlock = newBlock;  // the global variable "g_newlyMinedBlock" contains the new block
		printf("Bad Miner (#%d): Mined a new block #%d, with the hash 0x%x\n", miner_id, g_newlyMinedBlock.height, g_newlyMinedBlock.hash);
		pthread_cond_signal(&g_mined_new_block);
		pthread_mutex_unlock(&g_miner_lock);
	}
}
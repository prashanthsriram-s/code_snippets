#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Structs, Constants and Global variables
const unsigned int t = 10;
const unsigned int two_t = 20;
int firstAvailableBlockNo = 0;
int rootBlockNo = 0;
FILE *secondaryMem;
struct Ref
{
    int blockNumber;
    int index;
};
typedef struct Ref Ref;
const Ref invalidRef = {-1, -1};
struct MemCell
{
    int key;
    Ref ref1, ref2;
};
typedef struct MemCell MemCell;
struct Block
{
    MemCell memCells[20];
};
typedef struct Block Block;
//Some more global variables
Block *theRootBlock = NULL;
Ref theRootRef;
//BST structure:
// 1 key per block
// first cell has key, left child ref as ref1, right child ref as ref2
// second cell has block number as key, ref for parent as ref1, ref2 is left unused
// i.e
//  memCells[0].key = key
//         .ref1: lref
//         .ref2: rref
//  memCells[1].key = blockNumber
//         .ref1: pref
//         .ref2: undefined
//Ref functions for BST
int isNull(Ref r)
{
    if (r.blockNumber == -1)
        return 1;
    else
        return 0;
}
int isSameRef(Ref r1, Ref r2)
{
    return (r1.blockNumber == r2.blockNumber && r1.index == r2.index);
}
void makeNull(Ref *r)
{
    r->blockNumber = -1;
}

//Block functions
Block *diskRead(int bno, int *count)
{
    Block *b = (Block *)malloc(sizeof(Block));
    fseek(secondaryMem, bno * sizeof(Block), SEEK_SET);
    fread(b, sizeof(Block), 1, secondaryMem);
    (*count) += 10;
    return b;
}
Block *diskReadUpdate(int bno, Block *blockToReRead, int *count)
{
    fseek(secondaryMem, bno * sizeof(Block), SEEK_SET);
    fread(blockToReRead, sizeof(Block), 1, secondaryMem);
    (*count) += 10;
    return blockToReRead;
}
void diskWrite(Block *b, int bno, int *count)
{
    fseek(secondaryMem, bno * sizeof(Block), SEEK_SET);
    fwrite(b, sizeof(Block), 1, secondaryMem);
    (*count) += 10;
}
void freeIfPossible(Block *holder) //Frees it if it is not root
{
    if (holder != theRootBlock)
        free(holder);
}
Block *freeAndRead(Block *holder, int blockNumberToBeRead, int *count)
{
    //printf("Trying to free %d,root is %d, tbr is %d\n", holder->memCells[1].key,theRootRef.blockNumber,blockNumberToBeRead);
    if (holder->memCells[1].key != blockNumberToBeRead) //if reading is reqd
    {
        if (holder != theRootBlock)
        {
            free(holder);
            //printf("Freed holder block\n");
            holder = diskRead(blockNumberToBeRead, count);
            //printf("Read %d\n", blockNumberToBeRead);
        }
        else
        { // don't free root block as it is used in the caller function
            holder = diskRead(blockNumberToBeRead, count);
            //printf("Read %d\n", blockNumberToBeRead);
        }
    }
    (*count)++; // once in comparison
    //else printf("No need to read\n");
    return holder;
}
//BST functions
void insertBST(Block *rootBlock, Ref rootRef, int key, int *count)
{
    //printf("Trying to insert %d\n", key);
    if (isNull(rootRef)) // empty tree
    {
        Block *newBlock = (Block *)malloc(sizeof(Block));
        newBlock->memCells[0].key = key;       //key
        makeNull(&newBlock->memCells[0].ref1); //l child
        makeNull(&newBlock->memCells[0].ref2); //r child
        makeNull(&newBlock->memCells[1].ref1); // parent
        (*count) += 4;
        theRootBlock = newBlock;
        theRootRef.blockNumber = firstAvailableBlockNo;
        theRootBlock->memCells[1].key = firstAvailableBlockNo;
        (*count)++;
        diskWrite(theRootBlock, firstAvailableBlockNo++, count);
        //printf("Made root as %d in block %d\n", key, firstAvailableBlockNo-1);
        return;
    }
    Ref current = rootRef;
    Ref parent;
    while (!isNull(current))
    {
        parent = current;
        rootBlock = freeAndRead(rootBlock, current.blockNumber, count);
        //printf("key is %d, current block key is %d\n", key,  rootBlock->memCells[0].key);
        if (key <= rootBlock->memCells[0].key)
            current.blockNumber = rootBlock->memCells[0].ref1.blockNumber;
        else
            current.blockNumber = rootBlock->memCells[0].ref2.blockNumber;
        *(count) += 2; //once in comparison, once in assignment
        //printf("In loop, new curr is %d\n", current.blockNumber);
    }
    // Now current holds a null ref, parent points to a block to parent of should be inserted node.
    //printf("Finished loop. final parent is %d\n", parent.blockNumber);
    Block *newBlock = (Block *)malloc(sizeof(Block));
    newBlock->memCells[0].key = key;
    makeNull(&newBlock->memCells[0].ref1);
    makeNull(&newBlock->memCells[0].ref2);
    newBlock->memCells[1].ref1 = parent;
    newBlock->memCells[1].key = firstAvailableBlockNo;
    *(count) += 5;
    if (key <= rootBlock->memCells[0].key)
    {
        rootBlock->memCells[0].ref1.blockNumber = firstAvailableBlockNo;
    }
    else
        rootBlock->memCells[0].ref2.blockNumber = firstAvailableBlockNo;
    (*count) += 2;
    diskWrite(rootBlock, parent.blockNumber, count);
    diskWrite(newBlock, firstAvailableBlockNo, count);
    //printf("Inserted %d in block %d\n", key, firstAvailableBlockNo);
    firstAvailableBlockNo++;
}
Ref searchBST(Block *rootBlock, Ref rootRef, int key, int *count)
{
    //printf("Trying to search %d\n", key);
    if (isNull(rootRef))
    {
        return invalidRef;
    }
    Ref current = rootRef;
    Ref parent;
    //printf("Root ref was not null\n");
    while (!isNull(current))
    {
        parent = current;
        rootBlock = freeAndRead(rootBlock, current.blockNumber, count);
        if (key == rootBlock->memCells[0].key)
        {
            //printf("Found match\n");
            freeIfPossible(rootBlock);
            (*count)++;
            return current;
        }
        if (key < rootBlock->memCells[0].key)
        {
            current.blockNumber = rootBlock->memCells[0].ref1.blockNumber;
            (*count) += 2;
            //printf("%d was < than %d, so current is now:%d\n", key, rootBlock->memCells[0].key, current.blockNumber);
        }
        else
        {
            current.blockNumber = rootBlock->memCells[0].ref2.blockNumber;
            (*count) += 2;
            //printf("%d was > than %d, so current is now:%d\n", key, rootBlock->memCells[0].key, current.blockNumber);
        }
    }
    freeIfPossible(rootBlock);
    return invalidRef;
}

void deleteBST(Ref refToBlockToBeDeleted, int *count)
{
    // Deleting only severs the links to that block in the simulated secondary storage(file)
    //  and sets that memory to all 0s
    //  assumed that OS can take it back in real secondary storage
    if (isNull(refToBlockToBeDeleted))
        return;
    Block *blockToBeDeleted = diskRead(refToBlockToBeDeleted.blockNumber, count);
    if (isNull(blockToBeDeleted->memCells[0].ref1) && isNull(blockToBeDeleted->memCells[0].ref2)) //Leaf node
    {
        (*count) += 2;
        //printf("it iis a leaf node\n");
        if (blockToBeDeleted == theRootBlock)
        {
            //printf("It was root\n");
            free(theRootBlock);
            makeNull(&theRootRef);
            return;
        }
        int parentBlockNumber = blockToBeDeleted->memCells[1].ref1.blockNumber;
        (*count)++;
        memset(blockToBeDeleted, 0, sizeof(*blockToBeDeleted)); // making it all 0s
        diskWrite(blockToBeDeleted, refToBlockToBeDeleted.blockNumber, count);
        freeIfPossible(blockToBeDeleted);
        Block *parent = diskRead(parentBlockNumber, count);
        // if node was a right child, make parent's right pointer null and vice versa
        if (parent->memCells[0].ref2.blockNumber == refToBlockToBeDeleted.blockNumber)
            makeNull(&parent->memCells[0].ref2);
        else
            makeNull(&parent->memCells[0].ref1);
        (*count) += 2;
        diskWrite(parent, parentBlockNumber, count);
        freeIfPossible(parent);
        return;
    }
    else if (isNull(blockToBeDeleted->memCells[0].ref1) || isNull(blockToBeDeleted->memCells[0].ref2)) // one child only
    {
        (*count) += 4; // first 2 in if, then another 2 in this else if
        //printf("Had only one child\n");
        if (blockToBeDeleted == theRootBlock) // deleting node at root
        {
            //printf("it was root\n");
            int newBlockNumber;
            if (isNull(theRootBlock->memCells[0].ref2))                      // only has left child
                newBlockNumber = theRootBlock->memCells[0].ref1.blockNumber; // change the root to the new root
            else
                newBlockNumber = theRootBlock->memCells[0].ref2.blockNumber;
            (*count) += 2;
            free(theRootBlock);
            theRootBlock = diskRead(newBlockNumber, count);
            theRootRef.blockNumber = newBlockNumber;
            makeNull(&theRootBlock->memCells[1].ref1); // make parent null
            (*count)++;
            diskWrite(theRootBlock, theRootRef.blockNumber, count);
            return;
        }
        int parentBlockNumber = blockToBeDeleted->memCells[1].ref1.blockNumber;
        Ref lbref = blockToBeDeleted->memCells[0].ref1;
        Ref rbref = blockToBeDeleted->memCells[0].ref2;
        (*count) += 2;
        memset(blockToBeDeleted, 0, sizeof(*blockToBeDeleted)); // making it all 0s
        diskWrite(blockToBeDeleted, refToBlockToBeDeleted.blockNumber, count);
        freeIfPossible(blockToBeDeleted);
        Block *parent = diskRead(parentBlockNumber, count);
        // If it is a left child
        if (parent->memCells[0].ref1.blockNumber == refToBlockToBeDeleted.blockNumber) // Find if node is a left or right child and do accordingly
        {
            (*count)++;
            if (!isNull(lbref))
            {
                parent->memCells[0].ref1.blockNumber = lbref.blockNumber; //node->parent->left = node->left;
                (*count)++;
                diskWrite(parent, parentBlockNumber, count);
                freeIfPossible(parent);
                Block *left = diskRead(lbref.blockNumber, count);
                left->memCells[1].ref1.blockNumber = parentBlockNumber; //node->left->parent = node->parent;
                (*count)++;
                diskWrite(left, lbref.blockNumber, count);
                freeIfPossible(left);
                return;
            }
            else
            {
                parent->memCells[0].ref1.blockNumber = rbref.blockNumber; //node->parent->left = node->right;
                (*count)++;
                diskWrite(parent, parentBlockNumber, count);
                freeIfPossible(parent);
                Block *right = diskRead(rbref.blockNumber, count);
                right->memCells[1].ref1.blockNumber = parentBlockNumber; //node->right->parent = node->parent;
                (*count)++;
                diskWrite(right, rbref.blockNumber, count);
                freeIfPossible(right);
                return;
            }
        }
        else // if it is a right child
        {
            (*count)++; // due to if
            if (!isNull(lbref))
            {
                parent->memCells[0].ref2.blockNumber = lbref.blockNumber; //node->parent->right = node->left;
                (*count)++;
                diskWrite(parent, parentBlockNumber, count);
                freeIfPossible(parent);
                Block *left = diskRead(lbref.blockNumber, count);
                left->memCells[1].ref1.blockNumber = parentBlockNumber; //node->left->parent = node->parent;
                (*count)++;
                diskWrite(left, lbref.blockNumber, count);
                freeIfPossible(left);
                return;
            }
            else
            {
                parent->memCells[0].ref2.blockNumber = rbref.blockNumber; //node->parent->right = node->right;
                (*count)++;
                diskWrite(parent, parentBlockNumber, count);
                freeIfPossible(parent);
                Block *right = diskRead(rbref.blockNumber, count);
                right->memCells[1].ref1.blockNumber = parentBlockNumber; //node->right->parent = node->parent;
                (*count)++;
                diskWrite(right, rbref.blockNumber, count);
                freeIfPossible(right);
                return;
            }
        }
        return;
    }
    else
    { // if it has both child
        //Find successor
        //successor is left most child of right subtree since right child exists
        //printf("2 child case\n");
        (*count) += 4; // 2 in if, 2 in else if comparisons, which failed and landed here
        Ref current = blockToBeDeleted->memCells[0].ref2;
        (*count)++;
        freeIfPossible(blockToBeDeleted);
        Block *curr = diskRead(current.blockNumber, count);
        Ref parent;
        //printf("Root ref was not null\n");
        while (!isNull(current))
        {
            parent = current;
            curr = freeAndRead(curr, current.blockNumber, count);
            current.blockNumber = curr->memCells[0].ref1.blockNumber;
            (*count)++;
        }
        //curr is the suitable successor block. Now swap curr and blockToBeDeleted's contents and
        //delete curr, // parent is the ref to curr//current is ref to NULL
        int temp = curr->memCells[0].key;
        (*count)++;
        freeIfPossible(curr);
        blockToBeDeleted = diskRead(refToBlockToBeDeleted.blockNumber, count);
        blockToBeDeleted->memCells[0].key = temp;
        (*count)++;
        diskWrite(blockToBeDeleted, refToBlockToBeDeleted.blockNumber, count);
        freeIfPossible(blockToBeDeleted);
        deleteBST(parent, count); // delete the node at successor's position
    }
}
void deleteBSTByKey(Block *rootBlock, Ref rootRef, int key, int *count)
{
    //printf("Trying to delete %d\n", key);
    Ref r = searchBST(rootBlock, rootRef, key, count);
    if (isNull(r))
    {
        printf("%d not found in BST..\n", key);
        return;
    }
    printf("Deleting %d from the BST..\n", key);
    deleteBST(r, count);
}
void traverseBST(Ref curr, int *count)
{
    if (isNull(curr))
        return;
    Block *currBlock = diskRead(curr.blockNumber, count);
    Ref l = currBlock->memCells[0].ref1;
    Ref r = currBlock->memCells[0].ref2;
    (*count) += 2;
    freeIfPossible(currBlock);
    traverseBST(l, count);
    currBlock = diskRead(curr.blockNumber, count);
    printf("%d\n", currBlock->memCells[0].key);
    (*count)++;
    freeIfPossible(currBlock);
    traverseBST(r, count);
}
void simulateBST(char *insertListName, char *searchListName, char *deleteListName)
{
    FILE *insertList, *searchList, *deleteList;
    insertList = fopen(insertListName, "r");
    searchList = fopen(searchListName, "r");
    deleteList = fopen(deleteListName, "r");
    if (!insertList || !searchList || !deleteList)
        exit(1); // if file opening fails
    int counter = 0;
    int ic, sc, dc;
    ic = sc = dc = 0;
    makeNull(&theRootRef);
    printf("Simulating BST\n");
    //Insertion
    int num;
    while (1)
    {
        fscanf(insertList, "%d", &num);
        //printf("Main inserting %d-------\n", num);
        insertBST(theRootBlock, theRootRef, num, &counter);
        if (feof(insertList))
            break;
    }
    ic = counter;
    printf("BST Insertion completed\n");
    printf("BST Insertion count: %d\n", ic);
    //printf("Traversal after insertion:\n");
    //int tcounter=0;
    //traverseBST(theRootRef, &tcounter);
    while (1)
    {
        fscanf(searchList, "%d", &num);
        Ref r = searchBST(theRootBlock, theRootRef, num, &counter);
        if (!isNull(r))
            printf("Found %d in block %d\n", num, r.blockNumber);
        else
            printf("Cannot find %d\n", num);
        if (feof(searchList))
            break;
    }
    sc = counter - ic;
    printf("BST Search Completed\n");
    printf("BST Search counter : %d\n", sc);
    while (1)
    {
        fscanf(deleteList, "%d", &num);
        deleteBSTByKey(theRootBlock, theRootRef, num, &counter);
        if (feof(deleteList))
            break;
    }
    dc = counter - sc - ic;
    printf("BST Deletion completed\n");
    printf("BST Deletion counter: %d\n", dc);
    free(theRootBlock);
    //printf("After del, traversal\n");
    //traverseBST(theRootRef, &tcounter);
    printf("BST Total counter : %d\n", counter);
}
// B Tree Structure
// In the 2*t mem cell array. first 2t-1 cells are actually used. last cell for data about block itself
// In each of the first 2t-1 cells, key is key, ref1 is the left ref, ref2 is the parent ref
//i.e for a block x, i=0 to 2t-2
//  x->memCell[i].key for ith key index.(key_i[x]),
//  x->memCell[i].ref1 = C_i[x],
//  x->memCell[i].ref2 = parent(x)
// and for last key
// x->memCell[2t-1].key = num nodes, i.e n[x]
// x->memCell[2t-1].ref1 = C_[2t-1](x)
// x->memCell[2t-1].ref2.blocknumber = block number of the same block
// x->memCell[2t-2].ref2.index = isLeaf
int N(Block* b, int* count);
Ref leftChildInBTree(Block *b, int i, int *count)
{
    (*count)++;
    if (i >= 0 && i <= N(b, count))
        return b->memCells[i].ref1;
    else
        return invalidRef;
}
Ref rightChildInBTree(Block *b, int i, int *count)
{
    (*count)++;
    return b->memCells[i + 1].ref1;
}
void setLeftChildInBTree(Block *b, int i, Ref r, int *count)
{
    (*count)++;
    b->memCells[i].ref1 = r;
}
Ref parentInBTree(Block *b, int i, int *count)
{
    (*count)++;
    return b->memCells[i].ref2;
}
void setParentInBTree(Block *b, int i, Ref parRefm, int *count)
{
    (*count)++;
    b->memCells[i].ref2 = parRefm;
}
int isLeaf(Block *b, int *count)
{
    (*count)++;
    return b->memCells[2 * t - 1].ref2.index;
}
void setIsLeaf(Block *b, int isLeaf, int *count)
{
    (*count)++;
    b->memCells[2 * t - 1].ref2.index = isLeaf;
}
int N(Block *b, int *count)
{
    (*count)++;
    return b->memCells[2 * t - 1].key;
}
void setN(Block *b, int newN, int *count)
{
    (*count)++;
    b->memCells[2 * t - 1].key = newN;
}
int blockNumberFromBlockInBTree(Block *b, int *count)
{
    (*count)++;
    return b->memCells[2 * t - 1].ref1.blockNumber;
}
void setBlockNumber(Block *b, int no, int *count)
{
    (*count)++;
    b->memCells[2 * t - 1].ref1.blockNumber = no;
}
int Key(Block *b, int i, int *count)
{
    (*count)++;
    return b->memCells[i].key;
}
void setKey(Block *b, int i, int newK, int *count)
{
    (*count)++;
    b->memCells[i].key = newK;
}
//Search
Ref BTreeSearch(Block *rootBlock, int key, int *count)
{
    int i;
    int numNodes = N(rootBlock, count);
    for (i = 0; i < numNodes && key > Key(rootBlock, i, count); i++) // reach the first key greater/= than inserting key
        ;
    if (i < numNodes)
        if (key == Key(rootBlock, i, count))
            return (Ref){blockNumberFromBlockInBTree(rootBlock, count), i}; // found it
    if (isLeaf(rootBlock, count))
        return invalidRef; // if can't go further means not present
    // load the appropriate child into disk and recurse
    // go to C_i, i.e left of ith child
    int blockNumberToBeRead = leftChildInBTree(rootBlock, i, count).blockNumber;
    rootBlock = freeAndRead(rootBlock, blockNumberToBeRead, count);
    return BTreeSearch(rootBlock, key, count);
}
int BTreeSplitChild(int pbno, int index, Block *fullChild, int *count)
{
    //int pbno = blockNumberFromBlockInBTree(parent, count);
    //freeIfPossible(parent);
    // create a new block
    Block *newBlock = (Block *)malloc(sizeof(Block));
    setBlockNumber(newBlock, firstAvailableBlockNo++, count);
    // if child was leaf, then so will be the new block
    setIsLeaf(newBlock, isLeaf(fullChild, count), count);
    // new child will have t-1 nodes, full child will become t-1 and parent gets one extra
    setN(newBlock, t - 1, count);
    setN(fullChild, t - 1, count);
    for (int i = 0; i < t - 1; i++)
        setKey(newBlock, i, Key(fullChild, i + t, count), count); // transfer right half of keys to new Block
    // transfer the child pointers too
    if (!isLeaf(fullChild, count))
    {
        for (int i = 0; i < t; i++)
        {
            setLeftChildInBTree(newBlock, i, leftChildInBTree(fullChild, i + t, count), count);
            setParentInBTree(newBlock, i, parentInBTree(fullChild, i + t, count), count); // parent ptrs
        }
    }
    //  shift child ptr for parent
    Block *parent = diskRead(pbno, count);
    //parent = freeAndRead(parent, pbno, count);
    int n = N(parent, count);
    for (int i = n; i >= index + 1; i--)
        setLeftChildInBTree(parent, i + 1, leftChildInBTree(parent, i, count), count);
    setLeftChildInBTree(parent, index + 1, (Ref){blockNumberFromBlockInBTree(newBlock, count), 0}, count);
    //shift elements in parent
    for (int i = n - 1; i >= index; i--)
        setKey(parent, i + 1, Key(parent, i, count), count);
    setKey(parent, index, Key(fullChild, t - 1, count), count);
    setN(parent, n + 1, count);
    diskWrite(fullChild, blockNumberFromBlockInBTree(fullChild, count), count);
    diskWrite(newBlock, blockNumberFromBlockInBTree(newBlock, count), count);
    diskWrite(parent, blockNumberFromBlockInBTree(parent, count), count);
    int nbno = blockNumberFromBlockInBTree(newBlock, count);
    freeIfPossible(newBlock);
    freeIfPossible(parent);
    return nbno;
}
void BTreeInsertNonFull(Block *rootBlock, int key, int *count)
{

    if (isLeaf(rootBlock, count))
    {
        // leaf, so just insert at the right index
        int n = N(rootBlock, count);
        int i;
        for (i = n - 1; i >= 0 && key < Key(rootBlock, i, count); i--)
            setKey(rootBlock, i + 1, Key(rootBlock, i, count), count); // shift it right to insert
        setKey(rootBlock, i + 1, key, count);
        setN(rootBlock, n + 1, count);
        diskWrite(rootBlock, blockNumberFromBlockInBTree(rootBlock, count), count);
        freeIfPossible(rootBlock);
    }
    else
    { // find correct child and use recursion
        int n = N(rootBlock, count);
        int i;
        for (i = n - 1; i >= 0 && key < Key(rootBlock, i, count); i--)
            ; // find the correct i place
        // now read right of this ith key or the left ptr of i+1^th
        int blockNumberToBeRead = leftChildInBTree(rootBlock, i + 1, count).blockNumber;
        int rbno = blockNumberFromBlockInBTree(rootBlock, count);
        freeIfPossible(rootBlock);
        Block *childBlock = diskRead(blockNumberToBeRead, count);
        if (N(childBlock, count) == 2 * t - 1)
        {
            int nbno = BTreeSplitChild(rbno, i + 1, childBlock, count);
            rootBlock = diskRead(rbno, count);
            int decider = Key(rootBlock, i, count);
            freeIfPossible(rootBlock);
            if (key > decider) // decide which child to insert to , new or old
            {
                Block *temp = diskRead(nbno, count);
                BTreeInsertNonFull(temp, key, count);
                //freeIfPossible(temp);
                return;
            }
            //else
            BTreeInsertNonFull(childBlock, key, count);
            //freeIfPossible(childBlock);
        }
        BTreeInsertNonFull(childBlock, key, count);
    }
}
void insertBTree(Block *rootBlock, int key, int *count)
{
    if (N(rootBlock, count) == 2 * t - 1) // root is full, make new root
    {
        //printf("root full case\n");
        Block *newBlock = (Block *)malloc(sizeof(Block));
        //printf("New block created\n");
        theRootBlock = newBlock;
        //printf("theRoot is changed\n");
        setIsLeaf(newBlock, 0, count);
        setN(newBlock, 0, count);
        setBlockNumber(newBlock, firstAvailableBlockNo, count);
        rootBlockNo = firstAvailableBlockNo;
        setLeftChildInBTree(newBlock, 0, (Ref){blockNumberFromBlockInBTree(rootBlock, count), 0}, count);
        int n = N(rootBlock, count);
        for (int i = 0; i < n; i++)
            setParentInBTree(rootBlock, i, (Ref){firstAvailableBlockNo, 0}, count);
        diskWrite(rootBlock, blockNumberFromBlockInBTree(rootBlock, count), count);
        diskWrite(newBlock, firstAvailableBlockNo, count);
        firstAvailableBlockNo++;
        BTreeSplitChild(firstAvailableBlockNo - 1, 0, rootBlock, count);
        freeIfPossible(rootBlock);
        int temp = blockNumberFromBlockInBTree(newBlock, count);
        freeIfPossible(newBlock);
        newBlock = diskRead(temp, count); // reading again in case it was modified in splitting
        BTreeInsertNonFull(newBlock, key, count);
    }
    else
    {
        rootBlock = diskReadUpdate(blockNumberFromBlockInBTree(rootBlock, count), rootBlock, count);
        BTreeInsertNonFull(rootBlock, key, count);
    }
}
void traverseBTree(Ref rootRef, int *count)
{
    Block *block = diskRead(rootRef.blockNumber, count);
    if (isLeaf(block, count))
    {
        int n = N(block, count);
        for (int i = 0; i < n; i++)
            printf("%d in block %d, index %d\n", Key(block, i, count), rootRef.blockNumber, i);
        freeIfPossible(block);
        return;
    }
    else
    {
        int n = N(block, count);
        for (int i = 0; i < n; i++)
        {
            Ref c = leftChildInBTree(block, i, count);
            freeIfPossible(block);
            traverseBTree(c, count);
            block = diskRead(rootRef.blockNumber, count);
            printf("%d in block %d, index %d\n", Key(block, i, count), rootRef.blockNumber, i);
        }
        Ref c = leftChildInBTree(block, n, count);
        freeIfPossible(block);
        traverseBTree(c, count);
    }
}
void deleteBTree(Block *rootBlock, int key, int *count);
int findPredecessorAndDelete(Ref rootRef, int *count)
{
    // predecessor is just keep going right now that we are in the left node
    Block *block = diskRead(rootRef.blockNumber, count);
    if (isLeaf(block, count))
    {
        int n = N(block, count);
        int save = Key(block, n - 1, count); //save the last key
        //now delete the last key
        deleteBTree(block, save, count);
        return save; // block is freed in deleteBTree itself. no need to free again
    }
    else
    {
        Ref r2 = leftChildInBTree(block, N(block, count), count);
        freeIfPossible(block);
        return findPredecessorAndDelete(r2, count);
    }
}
int findSuccessorAndDelete(Ref rootRef, int *count)
{
    // successor is just keep going left of the right node. we are already in right node
    Block *block = diskRead(rootRef.blockNumber, count);
    if (isLeaf(block, count))
    {
        int save = Key(block, 0, count); //save the first key
        //now delete the first key
        deleteBTree(block, save, count);
        return save; // block is freed in deleteBTree itself. no need to free again
    }
    else
    {
        Ref r2 = leftChildInBTree(block, 0, count);
        freeIfPossible(block);
        return findPredecessorAndDelete(r2, count);
    }
}
Block *mergeBTreeNodes(int pbno, int pos, Ref y, Ref z, int *count)
{
    Block *rootBlock = diskRead(pbno, count);
    int nx = N(rootBlock, count);
    int key = Key(rootBlock, pos, count);
    for (int i = pos; i < nx - 1; i++)
    {
        setKey(rootBlock, i, Key(rootBlock, i + 1, count), count);
        setLeftChildInBTree(rootBlock, i + 1, leftChildInBTree(rootBlock, i + 2, count), count);
    }
    setN(rootBlock, nx - 1, count);
    diskWrite(rootBlock, blockNumberFromBlockInBTree(rootBlock, count), count);
    freeIfPossible(rootBlock);
    //2. put G at the end of y
    Block *yChild = diskRead(y.blockNumber, count);
    Block *zChild = diskRead(z.blockNumber, count);
    int ny = N(yChild, count);
    int nz = N(zChild, count);
    setKey(yChild, ny, key, count);
    //3. shift keys and children from z to y
    for (int i = 0; i < nz; i++)
    {
        setKey(yChild, i + ny + 1, Key(zChild, i, count), count);
        setLeftChildInBTree(yChild, i + ny + 1, leftChildInBTree(zChild, i, count), count);
    }
    setLeftChildInBTree(yChild, nz + ny + 1, leftChildInBTree(zChild, nz, count), count);
    //
    setN(yChild, 2 * t - 1, count);
    ny = N(yChild, count);
    // forget about z block now
    memset(zChild, 0, sizeof(Block)); //overwrite to 0. spaceassumed to be retaken by OS in actual secondary mem
    diskWrite(zChild, z.blockNumber, count);
    freeIfPossible(zChild);
    if(nx==1)
    {
        // now root is empty. make yCHild the new root
        theRootBlock = yChild;
        rootBlockNo = blockNumberFromBlockInBTree(yChild, count);
        theRootRef.blockNumber = rootBlockNo;
        // now destroy old root
        rootBlock = diskRead(pbno, count);
        memset(rootBlock, 0, sizeof(Block));
        diskWrite(rootBlock, pbno, count);
        freeIfPossible(rootBlock);
    }
    diskWrite(yChild, blockNumberFromBlockInBTree(yChild, count), count);
    return yChild; // returns merged block
}
void deleteBTree(Block *rootBlock, int key, int *count)
{
    if (isLeaf(rootBlock, count)) //CLRS Case-1
    {

        // first check if it is there or not
        int pos = -1;
        int n = N(rootBlock, count);
        if (rootBlock == theRootBlock && n == 2)
            return; // don't delete more from root. need not worry about this for non-root leaves, since this will be checked before descending
        for (int i = 0; i < n; i++)
            if (Key(rootBlock, i, count) == key)
            {
                pos = i;
                break;
            }
        if (pos == -1) // not present, can't delete
        {
            printf("Not present in the tree. can't delete\n");
            freeIfPossible(rootBlock);
            return;
        }
        //present at index pos.=>shift others over there
        for (int i = pos; i < n - 1; i++)
            setKey(rootBlock, i, Key(rootBlock, i + 1, count), count);
        setN(rootBlock, n - 1, count);
        diskWrite(rootBlock, blockNumberFromBlockInBTree(rootBlock, count), count);
        freeIfPossible(rootBlock);
    }
    else
    {
        //check if it is in this internal node itself
        int pos = -1;
        int n = N(rootBlock, count);
        for (int i = 0; i < n; i++)
            if (Key(rootBlock, i, count) == key)
            {
                pos = i;
                break;
            }
        if (pos != -1) //present in this internal node itself-CLRS Case-2
        {
            //k is present at index=pos. child y preceding k is left of key at pos
            Ref y = leftChildInBTree(rootBlock, pos, count);
            // y will always be a valid ref since this is an internal node
            Block *yChild = diskRead(y.blockNumber, count);
            int ny = N(yChild, count);
            freeIfPossible(yChild);
            if (ny >= t) //case 2a
            {
                // find predecessor k' of k in subtree rooted at y
                setKey(rootBlock, pos, findPredecessorAndDelete(y, count), count);
            }
            else
            {
                Ref z = leftChildInBTree(rootBlock, pos + 1, count);
                Block *zChild = diskRead(z.blockNumber, count);
                int nz = N(zChild, count);
                freeIfPossible(zChild);
                if (nz >= t) //Case 2b
                {
                    //find successor and do similar
                    setKey(rootBlock, pos, findSuccessorAndDelete(z, count), count);
                }
                else
                { //case 2c
                    //merging . both y and z only have t-1 keys.
                    //1 . in rootBlock, shift and destroy k
                    int rbno = blockNumberFromBlockInBTree(rootBlock, count);
                    freeIfPossible(rootBlock);
                    yChild = mergeBTreeNodes(rbno, pos, y, z, count);
                    // now recursively delete key from yChild
                    deleteBTree(yChild, key, count);
                    //freeIfPossible(yChild);
                }
            }
        }
        else
        { //CLRS case-3
            //determine the child which would have the key in its subtree
            int numNodes = N(rootBlock, count);
            int i;
            for (i = 0; i < numNodes && key > Key(rootBlock, i, count); i++)
                ; // reach the first key greater/= than the key
            // should go left to current i. but check if min amt of keys present
            Ref rChild = leftChildInBTree(rootBlock, i, count);
            int rbno = blockNumberFromBlockInBTree(rootBlock, count);
            freeIfPossible(rootBlock);
            Block *child = diskRead(rChild.blockNumber, count);
            if (N(child, count) == t - 1)
            {
                // check for immediate siblings
                // check for left
                rootBlock = diskRead(rbno, count);
                Ref lSibling = leftChildInBTree(rootBlock, i - 1, count);
                if (!isNull(lSibling))
                {
                    Block *lSiblingBlock = diskRead(lSibling.blockNumber, count);
                    int nl = N(lSiblingBlock, count);
                    if (nl == t)
                    {
                        // left sibling found
                        Ref temp = leftChildInBTree(lSiblingBlock, nl, count);
                        int donationFromSibling = Key(lSiblingBlock, nl - 1, count);
                        nl--;
                        setN(lSiblingBlock, nl, count);
                        diskWrite(lSiblingBlock, lSibling.blockNumber, count);
                        freeIfPossible(lSiblingBlock);
                        int keyFromParent = Key(rootBlock, i - 1, count);
                        setKey(rootBlock, i - 1, donationFromSibling, count);
                        diskWrite(rootBlock, blockNumberFromBlockInBTree(rootBlock, count), count);
                        freeIfPossible(rootBlock);
                        int nc = N(child, count);
                        for (int j = nc; j > 0; j--)
                        {
                            setKey(child, i, Key(child, i - 1, count), count);
                            setLeftChildInBTree(child, i + 1, leftChildInBTree(child, i, count), count);
                        }
                        setLeftChildInBTree(child, 1, leftChildInBTree(child, 0, count), count);
                        setLeftChildInBTree(child, 0, temp, count);
                        setKey(child, 0, keyFromParent, count);
                        setN(child, nc + 1, count);
                        nc++;
                        diskWrite(child, blockNumberFromBlockInBTree(child, count), count);
                        deleteBTree(child, key, count);
                        return;
                        //freeIfPossible(child);
                    }
                    else
                    {
                        //check for right sibling
                        freeIfPossible(lSiblingBlock);
                        Ref rSibling = leftChildInBTree(rootBlock, i + 1, count);
                        if (!isNull(rSibling))
                        {
                            Block *rSiblingBlock = diskRead(rSibling.blockNumber, count);
                            Ref temp = leftChildInBTree(rSiblingBlock, 0, count);
                            int donationFromSibling = Key(rSiblingBlock, 0, count);
                            int nr = N(rSiblingBlock, count);
                            for (int j = 0; j < nr - 1; j++)
                            {
                                setKey(rSiblingBlock, i, Key(rSiblingBlock, i + 1, count), count);
                                setLeftChildInBTree(rSiblingBlock, i, leftChildInBTree(rSiblingBlock, i + 1, count), count);
                            }
                            setLeftChildInBTree(rSiblingBlock, nr - 1, leftChildInBTree(rSiblingBlock, nr, count), count);
                            nr--;
                            setN(rSiblingBlock, nr, count);
                            diskWrite(rSiblingBlock, rSibling.blockNumber, count);
                            freeIfPossible(rSiblingBlock);
                            int keyFromParent = Key(rootBlock, i, count);
                            setKey(rootBlock, i, donationFromSibling, count);
                            diskWrite(rootBlock, blockNumberFromBlockInBTree(rootBlock, count), count);
                            freeIfPossible(rootBlock);
                            int nc = N(child, count);
                            setKey(child, nc, keyFromParent, count);
                            setLeftChildInBTree(child, nc + 1, temp, count);
                            nc++;
                            setN(child, nc, count);
                            diskWrite(child, rChild.blockNumber, count);
                            deleteBTree(child, key, count);
                            return;
                        }
                        else
                        { //case 3b
                            //merge child and one sibling of it by pulling a node from root as median
                            // lets merge child and its l sibling
                            freeIfPossible(child);
                            //int rbno = blockNumberFromBlockInBTree(rootBlock, count);
                            freeIfPossible(rootBlock);
                            Block* newLSibling = mergeBTreeNodes(rbno, i, lSibling, rChild, count);
                            diskWrite(newLSibling, blockNumberFromBlockInBTree(newLSibling, count), count);
                            deleteBTree(newLSibling, key, count);
                        }
                    }
                }
                else
                {
                    Ref rSibling = leftChildInBTree(rootBlock, i + 1, count);
                    if (!isNull(rSibling))
                    {
                        Block *rSiblingBlock = diskRead(rSibling.blockNumber, count);
                        Ref temp = leftChildInBTree(rSiblingBlock, 0, count);
                        int donationFromSibling = Key(rSiblingBlock, 0, count);
                        int nr = N(rSiblingBlock, count);
                        for (int j = 0; j < nr - 1; j++)
                        {
                            setKey(rSiblingBlock, i, Key(rSiblingBlock, i + 1, count), count);
                            setLeftChildInBTree(rSiblingBlock, i, leftChildInBTree(rSiblingBlock, i + 1, count), count);
                        }
                        setLeftChildInBTree(rSiblingBlock, nr - 1, leftChildInBTree(rSiblingBlock, nr, count), count);
                        nr--;
                        setN(rSiblingBlock, nr, count);
                        diskWrite(rSiblingBlock, rSibling.blockNumber, count);
                        freeIfPossible(rSiblingBlock);
                        int keyFromParent = Key(rootBlock, i, count);
                        setKey(rootBlock, i, donationFromSibling, count);
                        diskWrite(rootBlock, blockNumberFromBlockInBTree(rootBlock, count), count);
                        freeIfPossible(rootBlock);
                        int nc = N(child, count);
                        setKey(child, nc, keyFromParent, count);
                        setLeftChildInBTree(child, nc + 1, temp, count);
                        nc++;
                        setN(child, nc, count);
                        diskWrite(child, rChild.blockNumber, count);
                        deleteBTree(child, key, count);
                        return;
                    }
                    else
                    { //case 3b - merge child and r sibling
                        freeIfPossible(child);
                        //int rbno = blockNumberFromBlockInBTree(rootBlock, count);
                        freeIfPossible(rootBlock);
                        Block* newChild = mergeBTreeNodes(rbno, i, rChild, rSibling, count);
                        diskWrite(newChild, blockNumberFromBlockInBTree(newChild, count), count);
                        deleteBTree(newChild, key, count);
                    }
                }
            } // just descend down
            deleteBTree(child, key, count);
        }
    }
}
void simulateBTree(char *insertListName, char *searchListName, char *deleteListName)
{
    FILE *insertList, *searchList, *deleteList;
    insertList = fopen(insertListName, "r");
    searchList = fopen(searchListName, "r");
    deleteList = fopen(deleteListName, "r");
    if (!insertList || !searchList || !deleteList)
        exit(1); // if file opening fails
    int counter = 0;
    int ic, sc, dc;
    ic = sc = dc = 0;
    makeNull(&theRootRef);
    firstAvailableBlockNo = 0;
    printf("Simulating BTREE\n");
    //Create BTree
    theRootBlock = (Block *)malloc(sizeof(Block));
    setIsLeaf(theRootBlock, 1, &counter);
    setN(theRootBlock, 0, &counter);
    diskWrite(theRootBlock, firstAvailableBlockNo++, &counter);
    theRootRef.blockNumber = firstAvailableBlockNo - 1;
    rootBlockNo = theRootRef.blockNumber;
    //Insertion
    int num;
    while (1)
    {
        fscanf(insertList, "%d", &num);
        insertBTree(theRootBlock, num, &counter);
        if (feof(insertList))
            break;
    }
    ic = counter;
    printf("BTREE Insertion completed\n");
    printf("BTREE Insertion count: %d\n", ic);
    while (1)
    {
        fscanf(searchList, "%d", &num);
        Ref r = BTreeSearch(theRootBlock, num, &counter);
        if (!isNull(r))
            printf("Found %d in block %d, index %d\n", num, r.blockNumber, r.index);
        else
            printf("Cannot find %d\n", num);
        if (feof(searchList))
            break;
    }
    sc = counter - ic;
    printf("BTREE Search Completed\n");
    printf("BTREE Search counter : %d\n", sc); 
    while (1)
    {
        fscanf(deleteList, "%d", &num);
        deleteBTree(theRootBlock, num, &counter);
        if(feof(deleteList))break;
    }
    dc = counter - sc - ic;
    printf("BTREE Deletion completed\n");
    printf("BTREE Deletion counter: %d\n", dc);
    //printf("After del, traversal\n");
    //theRootRef.blockNumber = blockNumberFromBlockInBTree(theRootBlock, &counter);
    //traverseBTree(theRootRef, &counter);
    printf("BTREE Total counter : %d\n", counter);
}
int main()
{
    secondaryMem = fopen("secondaryMemory.bin", "wb+");
    if (!secondaryMem)
        exit(1); // if file opening failed
    simulateBST("insertList.txt", "searchList.txt", "deleteList.txt");
    fclose(secondaryMem);
    remove("secondaryMemory.bin");
    secondaryMem = fopen("secondaryMemory.bin", "wb+");
    simulateBTree("insertList.txt", "searchList.txt", "deleteList.txt");
    fclose(secondaryMem);
    remove("secondaryMemory.bin");
    return 0;
}

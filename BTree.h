#ifndef BTREE_H
#define BTREE_H

#include <stdlib.h>
#include <vector>
#include <string>

using namespace std;

// A BTree node
class BTreeNode
{
	public:
		string *keys;  // An array of keys
		int t;      // Minimum degree (defines the range for number of keys)
		BTreeNode **C; // An array of child pointers
		int n;     // Current number of keys
		bool leaf; // Is true when node is leaf. Otherwise false
		vector<int> *offsets;	//array of vectors containing offsets. same size as n
		BTreeNode(int _t, bool _leaf);   // Constructor
		void insertNonFull(string k, int off);
		void splitChild(int i, BTreeNode *y);
		void traverse();
		//will return vector of seekg offsets
		BTreeNode* search(string k);
	friend class BTree;
};
 
// A BTree
class BTree
{
	public:
		BTreeNode *root; // Pointer to root node
		int t;  // Minimum degree
		int distinctvals;
		string table;
		string column;

		// Constructor (Initializes tree as empty)
		BTree(int _t, string tbl, string col)
		{  root = NULL;  t = _t; table = tbl; column = col; distinctvals = 0;}
	 
		// function to traverse the tree
		void traverse()
		{  if (root != NULL) root->traverse(); }
	 
		// function to search a key in this tree
		BTreeNode* search(string k)
		{  return (root == NULL)? NULL : root->search(k); }
	 
		// The main function that inserts a new key in this B-Tree
		void insert(string k, int off);
};

#endif
#pragma once
/**
 * https://www.cnblogs.com/skywang12345/p/3624291.html
 * C++ 语言: 红黑树
 *
 * @author skywang
 * @modified by Menooker
 * @date 2013/11/07
 * @modified date 2018/10/26
 */

#ifndef _RED_BLACK_TREE_HPP_
#define _RED_BLACK_TREE_HPP_ 
#include <DogeeBase.h>
#include <DogeeMacro.h>
#include <iomanip>
#include <iostream>
namespace Dogee
{
	namespace RBTreeNameSpace
	{
		enum RBTColor { RED, BLACK };
	}

	template <class T, class TV>
	class RBTNode : public DObject {
	public:
		
		DefBegin(DObject);
		Def(key, T);            // 关键字(键值)
		Def(value, TV);
		Def(color, RBTreeNameSpace::RBTColor);    // 颜色
		Def(left, Ref<RBTNode>);    // 左孩子
		Def(right, Ref<RBTNode>);    // 右孩子
		Def(parent, Ref<RBTNode>); // 父结点
		DefEnd();

		RBTNode(ObjectKey okey) :DObject(okey) {}
		RBTNode(ObjectKey okey, T key, TV value, RBTreeNameSpace::RBTColor c, Ref<RBTNode> p, Ref<RBTNode> l, Ref<RBTNode> r) :
			DObject(okey) {
			self->key = key;
			self->value = value;
			self->color = c;
			self->parent = p;
			self->left = l;
			self->right = r;
		}
	};

	template <class T, class TV>
	class DMap:public DObject {
	private:
		DefBegin(DObject);
		Def(mRoot, Ref<RBTNode<T, TV>>);    // 根结点
		
	public:
		DefEnd();
		DMap(ObjectKey okey);
		~DMap();

		// (递归实现)查找"红黑树"中键值为key的节点
		Ref<RBTNode<T, TV>>  search(const T& key);

		// 查找最小结点：返回最小结点的键值。
		Ref<RBTNode<T, TV>> minimum();
		// 查找最大结点：返回最大结点的键值。
		Ref<RBTNode<T, TV>> maximum();

		// 找结点(x)的后继结点。即，查找"红黑树中数据值大于该结点"的"最小结点"。
		Ref<RBTNode<T, TV>>  successor(Ref<RBTNode<T, TV>> x);
		// 找结点(x)的前驱结点。即，查找"红黑树中数据值小于该结点"的"最大结点"。
		Ref<RBTNode<T, TV>>  predecessor(Ref<RBTNode<T, TV>> x);

		// 将结点(key为节点键值)插入到红黑树中
		void insert(T key, TV value);

		// 删除结点(key为节点键值)
		void remove(T key);

		// 销毁红黑树
		void destroy();

		// 打印红黑树
		void print();


		Ref<RBTNode<T, TV>> upperBound(const T& key) 
		{
			Ref<RBTNode<T, TV>> ub;
			upperBound(self->mRoot.get(), key, ub);
			return ub;
		}

		Ref<RBTNode<T, TV>> lowerBound(const T& key) 
		{
			Ref<RBTNode<T, TV>> lb;
			lowerBound(self->mRoot.get(), key, lb);
			return lb;
		}
	private:


		void upperBound(Ref<RBTNode<T, TV>>  root, const T& key, Ref<RBTNode<T, TV>>& ub) const;

		void lowerBound(Ref<RBTNode<T, TV>>  root, const T& key, Ref<RBTNode<T, TV>>& lb) const;

		// (递归实现)查找"红黑树x"中键值为key的节点
		Ref<RBTNode<T, TV>>  search(Ref<RBTNode<T, TV>>  x, const T& key) const;

		// 查找最小结点：返回tree为根结点的红黑树的最小结点。
		Ref<RBTNode<T, TV>>  minimum(Ref<RBTNode<T, TV>>  tree);
		// 查找最大结点：返回tree为根结点的红黑树的最大结点。
		Ref<RBTNode<T, TV>>  maximum(Ref<RBTNode<T, TV>>  tree);

		// 左旋
		void leftRotate(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  x);
		// 右旋
		void rightRotate(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  y);
		// 插入函数
		void insertNode(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  node);
		// 插入修正函数
		void insertFixUp(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  node);
		// 删除函数
		void remove(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>> node);
		// 删除修正函数
		void removeFixUp(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>> node, Ref<RBTNode<T, TV>> parent);

		// 销毁红黑树
		void destroy(Ref<RBTNode<T, TV>>  tree);

		// 打印红黑树
		void print(Ref<RBTNode<T, TV>>  tree, T key, int direction);

#define rb_parent(r)   ((r)->parent)
#define rb_color(r) ((r)->color)
#define rb_is_red(r)   ((r)->color==RBTreeNameSpace::RED)
#define rb_is_black(r)  ((r)->color==RBTreeNameSpace::BLACK)
#define rb_set_black(r)  do { (r)->color = RBTreeNameSpace::BLACK; } while (0)
#define rb_set_red(r)  do { (r)->color = RBTreeNameSpace::RED; } while (0)
#define rb_set_parent(r,p)  do { (r)->parent = (p); } while (0)
#define rb_set_color(r,c)  do { (r)->color = (c); } while (0)
	};

	/*
	 * 构造函数
	 */
	template <class T, class TV>
	DMap<T, TV>::DMap(ObjectKey okey) :DObject(okey)
	{
		self->mRoot = nullptr;
	}

	/*
	 * 析构函数
	 */
	template <class T, class TV>
	DMap<T, TV>::~DMap()
	{
		destroy();
	}

	/*
	 * (递归实现)查找"红黑树x"中键值为key的节点
	 */
	template <class T, class TV>
	Ref<RBTNode<T, TV>>  DMap<T, TV>::search(Ref<RBTNode<T, TV>>  x, const T& key) const
	{
		if (x == nullptr || x->key == key)
			return x;

		if (key < x->key)
			return search(x->left, key);
		else
			return search(x->right, key);
	}

	template <class T, class TV>
	void DMap<T, TV>::upperBound(Ref<RBTNode<T, TV>>  root, const T& key, Ref<RBTNode<T, TV>>& ub) const
	{
		if (root == nullptr) {
			return;
		}
		if (root->key > key) {
			ub = root;
			upperBound(root->left.get(), key, ub);
		}
		else {
			upperBound(root->right.get(), key, ub);
		}
	}

	template <class T, class TV>
	void DMap<T, TV>::lowerBound(Ref<RBTNode<T, TV>>  root, const T& key, Ref<RBTNode<T, TV>>& lb) const
	{
		if (root == nullptr) {
			return;
		}
		if (root->key >= key) {
			lb = root;
			lowerBound(root->left.get(), key, lb);
		}
		else {
			lowerBound(root->right.get(), key, lb);
		}
	}

	template <class T, class TV>
	Ref<RBTNode<T, TV>>  DMap<T, TV>::search(const T& key)
	{
		return search(self->mRoot, key);
	}

	/*
	 * 查找最小结点：返回tree为根结点的红黑树的最小结点。
	 */
	template <class T, class TV>
	Ref<RBTNode<T, TV>>  DMap<T, TV>::minimum(Ref<RBTNode<T, TV>>  tree)
	{
		if (tree == nullptr)
			return nullptr;

		while (tree->left != nullptr)
			tree = tree->left;
		return tree;
	}

	template <class T, class TV>
	Ref<RBTNode<T, TV>> DMap<T, TV>::minimum()
	{
		Ref<RBTNode<T, TV>> p = minimum(self->mRoot);
		return p;
	}

	/*
	 * 查找最大结点：返回tree为根结点的红黑树的最大结点。
	 */
	template <class T, class TV>
	Ref<RBTNode<T, TV>>  DMap<T, TV>::maximum(Ref<RBTNode<T, TV>>  tree)
	{
		if (tree == nullptr)
			return nullptr;

		while (tree->right != nullptr)
			tree = tree->right;
		return tree;
	}

	template <class T, class TV>
	Ref<RBTNode<T, TV>> DMap<T, TV>::maximum()
	{
		Ref<RBTNode<T, TV>> p = maximum(self->mRoot);
		return p;
	}

	/*
	 * 找结点(x)的后继结点。即，查找"红黑树中数据值大于该结点"的"最小结点"。
	 */
	template <class T, class TV>
	Ref<RBTNode<T, TV>>  DMap<T, TV>::successor(Ref<RBTNode<T, TV>> x)
	{
		// 如果x存在右孩子，则"x的后继结点"为 "以其右孩子为根的子树的最小结点"。
		if (x->right != nullptr)
			return minimum(x->right);

		// 如果x没有右孩子。则x有以下两种可能：
		// (01) x是"一个左孩子"，则"x的后继结点"为 "它的父结点"。
		// (02) x是"一个右孩子"，则查找"x的最低的父结点，并且该父结点要具有左孩子"，找到的这个"最低的父结点"就是"x的后继结点"。
		Ref<RBTNode<T, TV>>  y = x->parent;
		while ((y != nullptr) && (x == y->right))
		{
			x = y;
			y = y->parent;
		}

		return y;
	}

	/*
	 * 找结点(x)的前驱结点。即，查找"红黑树中数据值小于该结点"的"最大结点"。
	 */
	template <class T, class TV>
	Ref<RBTNode<T, TV>>  DMap<T, TV>::predecessor(Ref<RBTNode<T, TV>> x)
	{
		// 如果x存在左孩子，则"x的前驱结点"为 "以其左孩子为根的子树的最大结点"。
		if (x->left != nullptr)
			return maximum(x->left);

		// 如果x没有左孩子。则x有以下两种可能：
		// (01) x是"一个右孩子"，则"x的前驱结点"为 "它的父结点"。
		// (01) x是"一个左孩子"，则查找"x的最低的父结点，并且该父结点要具有右孩子"，找到的这个"最低的父结点"就是"x的前驱结点"。
		Ref<RBTNode<T, TV>>  y = x->parent;
		while ((y != nullptr) && (x == y->left))
		{
			x = y;
			y = y->parent;
		}

		return y;
	}

	/*
	 * 对红黑树的节点(x)进行左旋转
	 *
	 * 左旋示意图(对节点x进行左旋)：
	 *      px                              px
	 *     /                               /
	 *    x                               y
	 *   /  \      --(左旋)-->           / \                #
	 *  lx   y                          x  ry
	 *     /   \                       /  \
	 *    ly   ry                     lx  ly
	 *
	 *
	 */
	template <class T, class TV>
	void DMap<T, TV>::leftRotate(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  x)
	{
		// 设置x的右孩子为y
		Ref<RBTNode<T, TV>> y = x->right;

		// 将 “y的左孩子” 设为 “x的右孩子”；
		// 如果y的左孩子非空，将 “x” 设为 “y的左孩子的父亲”
		auto yv = y->left.get();
		x->right = yv;
		if (y->left != nullptr)
			y->left->parent = x;

		// 将 “x的父亲” 设为 “y的父亲”
		auto xv = x->parent.get();
		y->parent = xv;

		if (xv == nullptr)
		{
			root = y;            // 如果 “x的父亲” 是空节点，则将y设为根节点
			self->mRoot = y;
		}
		else
		{
			if (xv->left == x)
				xv->left = y;    // 如果 x是它父节点的左孩子，则将y设为“x的父节点的左孩子”
			else
				xv->right = y;    // 如果 x是它父节点的左孩子，则将y设为“x的父节点的左孩子”
		}

		// 将 “x” 设为 “y的左孩子”
		y->left = x;
		// 将 “x的父节点” 设为 “y”
		x->parent = y;
	}

	/*
	 * 对红黑树的节点(y)进行右旋转
	 *
	 * 右旋示意图(对节点y进行左旋)：
	 *            py                               py
	 *           /                                /
	 *          y                                x
	 *         /  \      --(右旋)-->            /  \                     #
	 *        x   ry                           lx   y
	 *       / \                                   / \                   #
	 *      lx  rx                                rx  ry
	 *
	 */
	template <class T, class TV>
	void DMap<T, TV>::rightRotate(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  y)
	{
		// 设置x是当前节点的左孩子。
		Ref<RBTNode<T, TV>> x = y->left;

		// 将 “x的右孩子” 设为 “y的左孩子”；
		// 如果"x的右孩子"不为空的话，将 “y” 设为 “x的右孩子的父亲”
		auto tempv = x->right.get();
		y->left = tempv;
		if (x->right != nullptr)
			x->right->parent = y;

		// 将 “y的父亲” 设为 “x的父亲”
		auto yv = y->parent.get();
		x->parent = yv;
		if (yv == nullptr)
		{
			root = x;            // 如果 “y的父亲” 是空节点，则将x设为根节点
			self->mRoot = x;
		}
		else
		{
			if (y == yv->right)
				yv->right = x;    // 如果 y是它父节点的右孩子，则将x设为“y的父节点的右孩子”
			else
				yv->left = x;    // (y是它父节点的左孩子) 将x设为“x的父节点的左孩子”
		}

		// 将 “y” 设为 “x的右孩子”
		x->right = y;

		// 将 “y的父节点” 设为 “x”
		y->parent = x;
	}

	/*
	 * 红黑树插入修正函数
	 *
	 * 在向红黑树中插入节点之后(失去平衡)，再调用该函数；
	 * 目的是将它重新塑造成一颗红黑树。
	 *
	 * 参数说明：
	 *     root 红黑树的根
	 *     node 插入的结点        // 对应《算法导论》中的z
	 */
	template <class T, class TV>
	void DMap<T, TV>::insertFixUp(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  node)
	{
		Ref<RBTNode<T, TV>> parent, gparent;

		// 若“父节点存在，并且父节点的颜色是红色”
		while ((parent = rb_parent(node))!=nullptr && rb_is_red(parent))
		{
			gparent = rb_parent(parent);

			//若“父节点”是“祖父节点的左孩子”
			if (parent == gparent->left)
			{
				// Case 1条件：叔叔节点是红色
				{
					Ref<RBTNode<T, TV>> uncle = gparent->right;
					if (uncle!=nullptr && rb_is_red(uncle))
					{
						rb_set_black(uncle);
						rb_set_black(parent);
						rb_set_red(gparent);
						node = gparent;
						continue;
					}
				}

				// Case 2条件：叔叔是黑色，且当前节点是右孩子
				if (parent->right == node)
				{
					Ref<RBTNode<T, TV>> tmp;
					leftRotate(root, parent);
					tmp = parent;
					parent = node;
					node = tmp;
				}

				// Case 3条件：叔叔是黑色，且当前节点是左孩子。
				rb_set_black(parent);
				rb_set_red(gparent);
				rightRotate(root, gparent);
			}
			else//若“z的父节点”是“z的祖父节点的右孩子”
			{
				// Case 1条件：叔叔节点是红色
				{
					Ref<RBTNode<T, TV>> uncle = gparent->left;
					if (uncle!=nullptr && rb_is_red(uncle))
					{
						rb_set_black(uncle);
						rb_set_black(parent);
						rb_set_red(gparent);
						node = gparent;
						continue;
					}
				}

				// Case 2条件：叔叔是黑色，且当前节点是左孩子
				if (parent->left == node)
				{
					Ref<RBTNode<T, TV>> tmp;
					rightRotate(root, parent);
					tmp = parent;
					parent = node;
					node = tmp;
				}

				// Case 3条件：叔叔是黑色，且当前节点是右孩子。
				rb_set_black(parent);
				rb_set_red(gparent);
				leftRotate(root, gparent);
			}
		}

		// 将根节点设为黑色
		rb_set_black(root);
	}

	/*
	 * 将结点插入到红黑树中
	 *
	 * 参数说明：
	 *     root 红黑树的根结点
	 *     node 插入的结点        // 对应《算法导论》中的node
	 */
	template <class T, class TV>
	void DMap<T, TV>::insertNode(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>>  node)
	{
		Ref<RBTNode<T, TV>> y = nullptr;
		Ref<RBTNode<T, TV>> x = root;

		// 1. 将红黑树当作一颗二叉查找树，将节点添加到二叉查找树中。
		while (x != nullptr)
		{
			y = x;
			if (node->key < x->key)
				x = x->left;
			else
				x = x->right;
		}

		node->parent = y;
		if (y != nullptr)
		{
			if (node->key < y->key)
				y->left = node;
			else
				y->right = node;
		}
		else
			root = node;

		// 2. 设置节点的颜色为红色
		node->color = RBTreeNameSpace::RED;

		// 3. 将它重新修正为一颗二叉查找树
		insertFixUp(root, node);
	}

	/*
	 * 将结点(key为节点键值)插入到红黑树中
	 *
	 * 参数说明：
	 *     tree 红黑树的根结点
	 *     key 插入结点的键值
	 */
	template <class T, class TV>
	void DMap<T, TV>::insert(T key, TV value)
	{
		Ref<RBTNode<T, TV>> z = nullptr;

		// 如果新建结点失败，则返回。
		z = NewObj<RBTNode<T, TV>>( key, value, RBTreeNameSpace::BLACK, nullptr, nullptr, nullptr);

		Ref< RBTNode<T, TV>> outroot = self->mRoot;
		insertNode(outroot, z);
		self->mRoot = outroot;
	}

	/*
	 * 红黑树删除修正函数
	 *
	 * 在从红黑树中删除插入节点之后(红黑树失去平衡)，再调用该函数；
	 * 目的是将它重新塑造成一颗红黑树。
	 *
	 * 参数说明：
	 *     root 红黑树的根
	 *     node 待修正的节点
	 */
	template <class T, class TV>
	void DMap<T, TV>::removeFixUp(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>> node, Ref<RBTNode<T, TV>> parent)
	{
		Ref<RBTNode<T, TV>> other;

		while ((!node || rb_is_black(node)) && node != root)
		{
			if (parent->left == node)
			{
				other = parent->right;
				if (rb_is_red(other))
				{
					// Case 1: x的兄弟w是红色的  
					rb_set_black(other);
					rb_set_red(parent);
					leftRotate(root, parent);
					other = parent->right;
				}
				if ((!other->left || rb_is_black(other->left)) &&
					(!other->right || rb_is_black(other->right)))
				{
					// Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的  
					rb_set_red(other);
					node = parent;
					parent = rb_parent(node);
				}
				else
				{
					if (!other->right || rb_is_black(other->right))
					{
						// Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。  
						rb_set_black(other->left);
						rb_set_red(other);
						rightRotate(root, other);
						other = parent->right;
					}
					// Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
					rb_set_color(other, rb_color(parent));
					rb_set_black(parent);
					rb_set_black(other->right);
					leftRotate(root, parent);
					node = root;
					break;
				}
			}
			else
			{
				other = parent->left;
				if (rb_is_red(other))
				{
					// Case 1: x的兄弟w是红色的  
					rb_set_black(other);
					rb_set_red(parent);
					rightRotate(root, parent);
					other = parent->left;
				}
				if ((!other->left || rb_is_black(other->left)) &&
					(!other->right || rb_is_black(other->right)))
				{
					// Case 2: x的兄弟w是黑色，且w的俩个孩子也都是黑色的  
					rb_set_red(other);
					node = parent;
					parent = rb_parent(node);
				}
				else
				{
					if (!other->left || rb_is_black(other->left))
					{
						// Case 3: x的兄弟w是黑色的，并且w的左孩子是红色，右孩子为黑色。  
						rb_set_black(other->right);
						rb_set_red(other);
						leftRotate(root, other);
						other = parent->left;
					}
					// Case 4: x的兄弟w是黑色的；并且w的右孩子是红色的，左孩子任意颜色。
					rb_set_color(other, rb_color(parent));
					rb_set_black(parent);
					rb_set_black(other->left);
					rightRotate(root, parent);
					node = root;
					break;
				}
			}
		}
		if (node)
			rb_set_black(node);
	}

	/*
	 * 删除结点(node)，并返回被删除的结点
	 *
	 * 参数说明：
	 *     root 红黑树的根结点
	 *     node 删除的结点
	 */
	template <class T, class TV>
	void DMap<T, TV>::remove(Ref<RBTNode<T, TV>>  &root, Ref<RBTNode<T, TV>> node)
	{
		Ref<RBTNode<T, TV>> child, *parent;
		RBTreeNameSpace::RBTColor color;

		// 被删除节点的"左右孩子都不为空"的情况。
		if ((node->left != nullptr) && (node->right != nullptr))
		{
			// 被删节点的后继节点。(称为"取代节点")
			// 用它来取代"被删节点"的位置，然后再将"被删节点"去掉。
			Ref<RBTNode<T, TV>> replace = node;

			// 获取后继节点
			replace = replace->right;
			while (replace->left != nullptr)
				replace = replace->left;

			// "node节点"不是根节点(只有根节点不存在父节点)
			if (rb_parent(node))
			{
				if (rb_parent(node)->left == node)
					rb_parent(node)->left = replace;
				else
					rb_parent(node)->right = replace;
			}
			else
				// "node节点"是根节点，更新根节点。
				root = replace;

			// child是"取代节点"的右孩子，也是需要"调整的节点"。
			// "取代节点"肯定不存在左孩子！因为它是一个后继节点。
			child = replace->right;
			parent = rb_parent(replace);
			// 保存"取代节点"的颜色
			color = rb_color(replace);

			// "被删除节点"是"它的后继节点的父节点"
			if (parent == node)
			{
				parent = replace;
			}
			else
			{
				// child不为空
				if (child)
					rb_set_parent(child, parent);
				parent->left = child;

				replace->right = node->right;
				rb_set_parent(node->right, replace);
			}

			replace->parent = node->parent;
			replace->color = node->color;
			replace->left = node->left;
			node->left->parent = replace;

			if (color == RBTreeNameSpace::BLACK)
				removeFixUp(root, child, parent);

			delete node;
			return;
		}

		if (node->left != nullptr)
			child = node->left;
		else
			child = node->right;

		parent = node->parent;
		// 保存"取代节点"的颜色
		color = node->color;

		if (child)
			child->parent = parent;

		// "node节点"不是根节点
		if (parent)
		{
			if (parent->left == node)
				parent->left = child;
			else
				parent->right = child;
		}
		else
			root = child;

		if (color == RBTreeNameSpace::BLACK)
			removeFixUp(root, child, parent);
		delete node;
	}

	/*
	 * 删除红黑树中键值为key的节点
	 *
	 * 参数说明：
	 *     tree 红黑树的根结点
	 */
	template <class T, class TV>
	void DMap<T, TV>::remove(T key)
	{
		Ref<RBTNode<T, TV>> node;

		// 查找key对应的节点(node)，找到的话就删除该节点
		if ((node = search(self->mRoot, key)) != nullptr)
			remove(self->mRoot, node);
	}

	/*
	 * 销毁红黑树
	 */
	template <class T, class TV>
	void DMap<T, TV>::destroy(Ref<RBTNode<T, TV>>  tree)
	{
		if (tree == nullptr)
			return;

		if (tree->left != nullptr)
			return destroy(tree->left);
		if (tree->right != nullptr)
			return destroy(tree->right);

		DelObj(tree);
	}

	template <class T, class TV>
	void DMap<T, TV>::destroy()
	{
		destroy(self->mRoot.get());
	}

	/*
	 * 打印"二叉查找树"
	 *
	 * key        -- 节点的键值
	 * direction  --  0，表示该节点是根节点;
	 *               -1，表示该节点是它的父结点的左孩子;
	 *                1，表示该节点是它的父结点的右孩子。
	 */
	template <class T, class TV>
	void DMap<T, TV>::print(Ref<RBTNode<T, TV>>  tree, T key, int direction)
	{
		if (tree != nullptr)
		{
			if (direction == 0)    // tree是根节点
				std::cout << std::setw(2) << tree->key.get() << "(B) is root" << std::endl;
			else                // tree是分支节点
				std::cout << std::setw(2) << tree->key.get() << (rb_is_red(tree) ? "(R)" : "(B)") << " is " << std::setw(2) << key << "'s " << std::setw(12) << (direction == 1 ? "right child" : "left child") << std::endl;

			print(tree->left, tree->key, -1);
			print(tree->right, tree->key, 1);
		}
	}

	template <class T, class TV>
	void DMap<T, TV>::print()
	{
		if (self->mRoot != nullptr)
			print(self->mRoot, self->mRoot->key, 0);
	}
}
#endif
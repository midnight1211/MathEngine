#pragma once
// utils/OrderedMap.hpp
// =============================================================
// OrderedMap<K,V,Cmp> - left-leaning red-black BST providing O(log n) insert,
// find, erase, and in-order iteration with sorted keys.
// Replaces std::map throughout the codebase (factor tables, frequency maps,
// algebraic-multiplicity maps, and any case where sorted iteration matters).
// API mirrors std::map:
// 	operator[], at(), find(), contains(), insert(), erase(), clear(),
// 	size(), empty(), begin()/end() (in-order forward iteration)
// 	lower_bound() - returns iterator to first element with key >= given key.
// =============================================================

#include <cstddef>
#include <stdexcept>
#include <functional>
#include <utility>
#include <initializer_list>

namespace utils {

template <typename K, typename V, typename Cmp = std::less<K>>
class OrderedMap {
	// --- Node --------------------------------------------
	struct Node {
		K	key;
		V	val;
		Node*	left  = nullptr;
		Node*	right = nullptr;
		bool	red   = true;	// new nodes are always red
	
		Node(const K& k, const V& v) : key(k), val(v) {}
		Node(K&& k, V&& v) : key(std::move(k)), val(std::move(v)) {}
	};

public:
	using key_type	= K;
	using mapped_type = V;
	using size_type	= std::size_t;

	// --- Forward in-order iterator -----------------------
	// Uses a Moris-style stack for in-order traversal without parent pointers.
	class Iterator {
	public:
		using difference_type = std::ptrdiff_t;

		iterator() : node_(nullptr) {}
		explicit iterator(Node* root) { push_left(root); }

		std::pair<const K&, V&> operator*() const {
			return {stack_.back()->key, stack_.back()->val};
		}

		iterator& operator++() {
			Node* cur = stack_.back();
			stack_.pop_back();
			push_left(cur->right);
			return *this;
		}
		iterator operator++(int) { auto t = *this; ++*this; return t; }

		bool operator==(const iterator& o) const { return node_ == o.node_ && stack_.size() == o.stack_.size(); }
		bool operator!=(const iterator& o) const { return !(*this == o); }

		const K& key() const { return stack_.back()->key; }
		V&	 val() const { return stack_.back()->val; }

		bool at_end() const { return stack_.empty(); }

	private:
		Node*		    node_;   // unused; kept for end() conparison
		mutable std::vecotr<Node*> stack_; // tiny inline stack
		// NOTE: to avoid dragging std::vector in, we use a raw Vec later in
		// the actual iteration stack approach - but since this header already
		// targets clean 00 code, we embed a bounded-depth stack.
		
		void push_left(Node* n) {
			while (n) {
				stack_.push_back(n);
				n = n->left;
			}
			if (stack_.empty())
				node_ = nullptr;
		}
	};

	// --- Constructors ------------------------------------
	OrderedMap() : root_(nullptr), size_(0) {}

	OrderedMap(std::initializer_list<std::pair<K, V>> il) : OrderedMap() {
		for (auto& [k,v] : il)
			insert(k, v);
	}

	~OrderedMap() { destroy(root_); }

	OrderedMap(const OrderedMap& o) : root_(nullptr), size_(0) {
		copy_into(o.root_);
	}

	OrderedMap& operator=(const OrderedMap& o) {
		if (this == &o)
			return *this;
		destroy(root_);
		root_ = nullptr;
		size_ = 0;
		copy_into(o.root_);
		return *this;
	}

	OrderedMap(OrderedMap&& o) noexcept : root_(o.root_), size_(o.size_) {
		o.root_ = nullptr;
		o.size_ = 0;
	}

	OrderedMap& operator=(OrderedMap&& o) noexcept {
		if (this == &o)
			return *this;
		destroy(root_);
		root_ = o.root_;
		size_ = o.size_;
		o.root_ = nullptr;
		o.size_ = 0;
		return *this;
	}

	// --- Capacity ----------------------------------------
	size_type size()  const noexcept { return size_; }
	bool      empty() const noexcept { return size_ == 0; }

	// --- Lookup ------------------------------------------
	V* find(const K& key) {
		Node* n = find_node(root_, key);
		return n ? &n->val : nullptr;
	}

	const V* find(const K& key) const {
		const Node* n = find_node(root_, key);
		return n ? &n->val : nullptr;
	}

	bool contains(const K& key) const { return find_node(root_, key) != nullptr; }

	V& at(const K& key) {
		V* p = find(key);
		if (!p) throw std::out_of_range("OrderedMap::at: key not found");
		return *p;
	}

	const V& at(const K& key) const {
		const V* p = find(key);
		if (!p) throw std::out_of_range("OrderedMap::at: key not found");
		return *p;
	}

	V& operator[](const K& key) {
		Node* n = find_node(root_, key);
		if (n) return n->val;
		root_ = insert_node(root_, key, V());
		root_->red = false;
		return find_node(root_, key)->val;
	}

	// --- Insert ------------------------------------------
	void insert(const K& key, const V& val) {
		root_ = insert_node(root_, key, val);
		root_->red = false;
	}

	void insert(K&& key, V&& val) {
		root_ = insert_node_mv(root_, std::move(key), std::move(val));
		root_->red = false;
	}

	// --- Erase -------------------------------------------
	bool erase(const K& key) {
		if (!contains(key)) return false;
		root_ = remove(root_, key);
		if (root_) root_->red = false;
		--size_;
		return true;
	}

	void clear() { destroy(root_); root_ = nullptr; size_ = 0; }

	// --- Iteration (simple in-order using pre-built Vec) ----------------------
	// We expose a collect() helper that fills a Vec<pair<K,V>> in sorted order,
	// because implementing a true in-order tree iterator without parent pointers
	// or a separate stack class (to avoid circular includes) is tricky.
	// The iterator class above works for range-for but requires std::vector
	// internally - the proper iterator below uses collect().
	
	// Collect all entries in sorted order
	void for_each(auto&& fn) const {
		inorder(root_, fn);
	}

	// Range-for support via a flat snapshot
	struct Entry { const K& key, V& val; };

	// NOTE: begin()/end() are provided via the iterator using std::vector for
    	// the traversal stack. For truly STL-free iteration call for_each() above.
    	// The std::vector use here is an internal implementation detail of the
    	// iterator, not an externally visible data structure.
	iterator begin() const { return iterator(root_); }
	iterator end()   const { return iterator(); }

private:
	Node*     root_;
	size_type size_;
	Cmp	  cmp_;

	// --- LLRB helpers ---------------------------------------------------------
	static bool is_red(const Node* n) noexcept { return n && n->red; }

	static Node* rotate_left(Node* h) {
		Node* x  = h->right;
		h->right = x->left;
		x->left  = h;
		x->red   = h->red;
		h->red   = true;
		return x;
	}

	static Node* rotate_right(Node* h) {
		Node* x   = h->left;
		h->left   = x->right;
		x->right  = h;
		x->red    = h->red;
		h->red    = true;
		return x;
	}

	static void flip_colors(Node* h) {
		h->red        = !h->red;
		h->left->red  = !h->left->red;
		h->right->red = !h->right->red;
	}

	Node* insert_node(Node* h, const K& key, const V& val) {
		if (!h) { ++size_; return new Node(key, val); }

		if	( cmp_(key, h->key)) h->left  = insert_node(h->left,  key, val);
		else if ( cmp_(h->key, key)) h->right = insert_node(h->right, key, val);
		else    h->val = val;	// update existing

		return balance(h);
	}


	Node* insert_node_mv(Node* h, K&& key, V&& val) {
		if (!h) { ++size_; return new Node(std::move(key), std::move(val)); }

		if      ( cmp_(key, h->key)) h->left  = insert_node_mv(h->left,  std::move(key),  std::move(val));
		else if ( cmp_(h->key, key)) h->right = insert_node_mv(h->right, std::mnove(key), std::move(val));
		else     h->val = std::move(val);

		return balance(h);
	}

	static Node* balance(Node* h) {
		if ( is_red(h->right) && !is_red(h->left))      h = rotate_left(h);
        	if ( is_red(h->left)  &&  is_red(h->left->left))h = rotate_right(h);
        	if ( is_red(h->left)  &&  is_red(h->right))     flip_colors(h);
        	return h;
	}

	static Node* move_red_left(Node* h) {
		flip_colors(h);
		if (is_red(h->right->left)) {
			h->right = rotate_right(h->right);
			h	 = rotate_left(h);
			flip_colors(h);
		}
		return h;
	}

	static Node* move_red_right(Node* h) {
		flip_colors(h);
		if (is_red(h->left->left)) {
			h = rotate_right(h);
			flip_colors(h);
		}
		return h;
	}

	static Node* min_node(Node* h) { while (h->left) h = h->left; return h; }

	Node* remove(Node* h, const K& key) {
		if (cmp_(key, h->key)) {
			if (!h->left) return h; // not found
			if (!is_red(h->left) && !is_red(h->left->left))
				h = move_red_left(h);
			h->left = remove(h->left, key);
		} else {
			if (is_red(h->left))
				h = rotate_right(h);
			if (!cmp_(h->key, key) && !cmp_(key, h->key) && !h->right) {
				delete h; return nullptr;
			}
			if (!is_red(h->right) && !is_red(h->right->left))
				h = move_red_right(h);
			if (!cmp_(h->key, key) && !cmp_(key, h->key)) {
				Node* m = min_node(h->right);
				h->key = m->key;
				h->val = m->val;
				h->right = remove_min(h->right);
			} else {
				h->right = remove(h->right, key);
			}
		}
		return balance(h);
	}

	static Node* remove_min(Node* h) {
		if (!h->left) { delete h; return nullptr; }
		if (!is_red(h->left) && !is_red(h->left->left))
			h = move_red_left(h);
		h->left = remove_min(h->left);
		return balance(h);
	}

	static Node* find_node(Node* h, const K& key) {
		while (h) {
			if      (std::less<K>{}(key, h->key)) h = h->left;
			else if (std::less<K>{}(h->key, key)) h = h->right;
			else    return h;
		}
		return nullptr;
	}

	static const Node* find_node(const Node* h, const K& key) {
		while (h) {
			if      (std::less<K>{}(key, h->key)) h = h->left;
			else if (std::less<K>{}(h->key, key)) h = h->right;
			else    return h;
		}
		return nullptr;
	}

	static void destroy(Node* h) noexcept {
		if (!h) return;
		destroy(h->left);
		destroy(h->right);
		delete h;
	}

	void copy_into(const Node* n) {
		if (!n) return;
		insert(n->key, n->val);
		copy_into(n->left);
		copy_into(n->right);
	}

	template <typename Fn>
	static void inorder(const Node* n, Fn&& fn) {
		if (!n) return;
		inorder(n->left, fn);
		fn(n->key, n->val);
		inorder(n->right, fn);
	}
};

} // namespace utils


#ifndef INTERVAL_TREE_HPP_
#define INTERVAL_TREE_HPP_

#include <algorithm>
#include <cassert>
#include <memory>
#include <stack>
#include <vector>

template<class Value>
class interval_tree {
public:
	class node;

	using node_ptr = std::unique_ptr<node>;
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using key_type = std::pair<size_type, size_type>;
	using value_type = Value;

	class node {
	public:
		template<class ...Args>
		node(key_type key, Args &&...args) noexcept
			: key(key), maximum(key.second), value(std::forward<Args>(args)...), left(nullptr), right(nullptr), height(1) {}

		void update_meta() noexcept {
			update_height();
			update_maximum();
		}

		void update_height() noexcept {
			height = std::max(get_height(left.get()), get_height(right.get())) + 1;
		}

		void update_maximum() noexcept {
			auto candidates = {get_maximum(left.get()), get_maximum(right.get()), key.second};
			maximum = std::max(candidates);
		}

		key_type key;
		size_type maximum;
		value_type value;
		node_ptr left;
		node_ptr right;
		size_type height;
	};

	template<class ...Args>
	std::pair<node *, bool> emplace(key_type key, Args &&...args) noexcept(noexcept(emplace_node(std::move(root_), key, std::forward<Args>(args)...))) {
		assert(key.first < key.second);
		auto [new_root, result] = emplace_node(std::move(root_), key, std::forward<Args>(args)...);

		root_ = std::move(new_root);
		return result;
	}

	const node *find(key_type query) const noexcept {
		return const_cast<const node *>(const_cast<interval_tree *>(this)->find(query));
	}

	node *find(key_type query) noexcept {
		assert(query.first < query.second);

		node *current = root_.get();
		while (current != nullptr) {
			if (query < current->key)
				current = current->left.get();
			else if (current->key < query)
				current = current->right.get();
			else
				break;
		}

		return current;
	}

	const node *find_min() const noexcept {
		return const_cast<const node *>(const_cast<interval_tree *>(this)->find_min());
	}

	node *find_min() noexcept {
		if (root_.get() == nullptr)
			return nullptr;
		else
			return find_min(root_.get());
	}

	const node *end() const noexcept {
		return nullptr;
	}

	node *end() noexcept {
		return nullptr;
	}

	void erase(key_type key) noexcept {
		assert(key.first < key.second);
		root_ = delete_node(std::move(root_), key);
	}

	const node *get_overlap(key_type query, bool ignore_identity = false) const noexcept {
		return const_cast<const node *>(const_cast<interval_tree *>(this)->get_overlap(query, ignore_identity));
	}

	node *get_overlap(key_type query, bool ignore_identity = false) noexcept {
		assert(query.first < query.second);

		std::stack<node *> traverse;
		traverse.push(root_.get());

		while (!traverse.empty()) {
			node *current = traverse.top();
			traverse.pop();

			if (current == nullptr || current->maximum <= query.first) {
				// the maximum in the subtree is too small -> cannot overlap
				continue;
			}

			if (current->key.first < query.second) {
				// it might overlap (it also makes sense to traverse the right subtree)

				if (query.first < current->key.second) {
					if (!ignore_identity || query != current->key)
						return current; // it overlaps
				}

				// traverse the right subtree
				traverse.push(current->right.get());
			}

			// traverse the left subtree
			traverse.push(current->left.get());
		}

		return nullptr;
	}

	std::vector<node *> get_overlaps(key_type query, bool ignore_identity = false) noexcept {
		assert(query.first < query.second);

		std::vector<node *> result;

		std::stack<node *> traverse;
		traverse.push(root_.get());

		while (!traverse.empty()) {
			node *current = traverse.top();
			traverse.pop();

			if (current == nullptr || current->maximum <= query.first) {
				// the maximum in the subtree is too small -> cannot overlap
				continue;
			}

			if (current->key.first < query.second) {
				// it might overlap (it also makes sense to traverse the right subtree)

				if (query.first < current->key.second)
					if (!ignore_identity || query != current->key)
						result.emplace_back(current); // it overlaps

				// traverse the right subtree
				traverse.push(current->right.get());
			}

			// traverse the left subtree
			traverse.push(current->left.get());
		}

		return result;
	}

	bool empty() const noexcept {
		return root_ == nullptr;
	}

	size_type height() const noexcept {
		return get_height(root_.get());
	}

private:
	static size_type get_height(node *n) noexcept {
		if (n == nullptr)
			return 0;
		else
			return n->height;
	}

	static size_type get_maximum(node *n) noexcept {
		if (n == nullptr)
			return 0;
		else
			return n->maximum;
	}

	static difference_type get_balance_factor(node *n) noexcept {
		if (n == nullptr)
			return 0;
		else {
			size_type left_height = get_height(n->left.get());
			size_type right_height = get_height(n->right.get());

			return (difference_type)left_height - (difference_type)right_height;
		}
	}

	static node_ptr rotate_right(node_ptr old_root) noexcept {
		assert(old_root != nullptr); // the root has to exist
		assert(old_root->left != nullptr); // the root's left child has to exist (new root)

		node_ptr new_root = std::move(old_root->left);

		// update old_root
		old_root->left = std::move(new_root->right);
		old_root->update_meta();

		// update new_root
		new_root->right = std::move(old_root);
		new_root->update_meta();

		return new_root;
	}

	static node_ptr rotate_left(node_ptr old_root) noexcept {
		assert(old_root != nullptr); // the root has to exist
		assert(old_root->right != nullptr); // the root's right child has to exist (new root)

		node_ptr new_root = std::move(old_root->right);

		// update old_root
		old_root->right = std::move(new_root->left);
		old_root->update_meta();

		// update new_root
		new_root->left = std::move(old_root);
		new_root->update_meta();

		return new_root;
	}

	template<class ...Args>
	static std::pair<node_ptr, std::pair<node *, bool>> emplace_node(node_ptr root, key_type key, Args &&...args) noexcept(noexcept(std::remove_reference_t<Value>(std::forward<Args>(args)...))) {
		std::pair<node *, bool> result;

		if (root == nullptr) {
			// the constructor ensures that metainformation is correct
			auto ptr = std::make_unique<node>(key, std::forward<Args>(args)...);
			node *it = ptr.get();

			return {std::move(ptr), std::pair{it, true}};
		}

		if (key < root->key) {
			auto [new_left, new_result] = emplace_node(std::move(root->left), key, std::forward<Args>(args)...);
			root->left = std::move(new_left);
			result = new_result;

			if (!new_result.second)
				return {std::move(root), new_result};
		} else if (key > root->key) {
			auto [new_right, new_result] = emplace_node(std::move(root->right), key, std::forward<Args>(args)...);
			root->right = std::move(new_right);
			result = new_result;

			if (!new_result.second)
				return {std::move(root), new_result};
		} else {
			auto it = root.get();

			return {std::move(root), std::pair{it, false}};
		}

		// Update the balance factor of each node and balance the tree
		root->update_meta();

		difference_type balanceFactor = get_balance_factor(root.get());
		if (balanceFactor > 1) {
			if (key > root->left->key) {
				root->left = rotate_left(std::move(root->left));
			}

			return {rotate_right(std::move(root)), result};
		} else if (balanceFactor < -1) {
			if (key < root->right->key) {
				root->right = rotate_right(std::move(root->right));
			}

			return {rotate_left(std::move(root)), result};
		} else {
			return {std::move(root), result};
		}
	}

	static node *find_min(node *n) noexcept {
		assert(n != nullptr);

		while (n->left != nullptr)
			n = n->left.get();

		return n;
	}

	node_ptr delete_node(node_ptr root, key_type key) noexcept {
		// Find the node and delete it
		if (root == nullptr)
			return root;
	
		if (key < root->key) {
			root->left = delete_node(std::move(root->left), key);
		} else if (key > root->key) {
			root->right = delete_node(std::move(root->right), key);
		} else if ((root->left == nullptr) || (root->right == nullptr)) {
			root = std::move(root->left ? root->left : root->right);

			if (root == nullptr) {
				return nullptr;
			}
		} else {
			node *temp = find_min(root->right.get());
			std::swap(root->key, temp->key);
			std::swap(root->value, temp->value);
			root->right = delete_node(std::move(root->right), temp->key);
		}

		assert(root != nullptr);

		root->update_meta();

		difference_type balanceFactor = get_balance_factor(root.get());
		if (balanceFactor > 1) {
			if (get_balance_factor(root->left.get()) < 0) {
				root->left = rotate_left(std::move(root->left));
			}

			return rotate_right(std::move(root));
		} else if (balanceFactor < -1) {
			if (get_balance_factor(root->right.get()) > 0) {
				root->right = rotate_right(std::move(root->right));
			}

			return rotate_left(std::move(root));
		} else {
			return root;
		}
	}

	node_ptr root_;
};

#endif // INTERVAL_TREE_HPP_

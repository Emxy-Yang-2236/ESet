#ifndef ESET_ESET_HPP
#define ESET_ESET_HPP
#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <memory>

enum class Color {
    RED,
    BLACK,
};

template<class Key,class Compare = std::less<Key>>
class ESet {
private:
    struct Node : public std::enable_shared_from_this<Node>{
        std::optional<Key> data;
        Color color;
        std::shared_ptr<Node> lc, rc;
        std::weak_ptr<Node> parent;
        int size{};

        std::shared_ptr<Node> get_parent() {
            return parent.lock();
        }
        std::shared_ptr<Node> get_grandparent() {
            return get_parent() == nullptr ? nullptr : get_parent()->get_parent();
        }
        std::shared_ptr<Node> get_uncle() {
            return get_grandparent() == nullptr ? nullptr :
                (get_grandparent()->lc == get_parent() ? get_grandparent()->rc : get_grandparent()->lc);
        }
        std::shared_ptr<Node> get_sibling() {
            return get_parent() == nullptr ? nullptr :
                (get_parent()->lc.get() == this ? get_parent()->rc : get_parent()->lc);
        }

        void switch_color() {
            color = color == Color::BLACK ? Color::RED : Color::BLACK;
        }

        Node(const Color c, std::shared_ptr<Node> p, std::optional<Key> d = std::nullopt, std::shared_ptr<Node> lc_ = nullptr, std::shared_ptr<Node> rc_ = nullptr)
            :data(d), color(c), parent(p), lc(lc_), rc(rc_), size(1) {}
        Node (Node && other)noexcept : data(std::move(other.data)), color(other.color), lc(std::move(other.lc)),rc(std::move(other.rc)),
            parent(std::move(other.parent)), size(other.size) {}

        [[nodiscard]] int getSize() const { return this->size; }
        void update() {
            if (this->lc == nullptr && this->rc == nullptr) {
                this->size = 0;
                return;
            }

            int left_size = (lc && lc->size > 0) ? lc->size : 0;
            int right_size = (rc && rc->size > 0) ? rc->size : 0;
            this->size = 1 + left_size + right_size;
        }
    };

    Compare compare_;

    std::shared_ptr<Node> root;
    std::shared_ptr<Node> NIL;

    void init_NIL(){
        NIL = std::make_shared<Node>(Color::BLACK, nullptr);
        NIL->size = 0;
        NIL->lc = NIL->rc = nullptr;
    }

    std::pair<std::shared_ptr<Node>, bool> insert_in(std::shared_ptr<Node> &node, std::shared_ptr<Node> &p, Key val) {
        if (node == NIL) {
            node = std::make_shared<Node>(Color::RED, p, val);
            node->lc = node->rc = NIL;
            return {node, true};
        }

        std::pair<std::shared_ptr<Node>, bool> ret = {nullptr,false};
        if (compare_(node->data.value() , val))
            ret = insert_in(node->rc, node, std::move(val));
        else if (compare_(val, node->data.value()))
            ret = insert_in(node->lc, node, std::move(val));
        else {
            ret = {node, false};
        }

        node->update();
        return ret;
    }

    void rotate_left(std::shared_ptr<Node> node) {
        auto child = node->rc;
        if (child == NIL) return;
        node->rc = child->lc;
        if (child->lc != NIL) child->lc->parent = node;

        auto parent = node->parent.lock();
        child->parent = parent;
        if (!parent || parent == NIL) {
            root = child;
            child->parent = NIL;
        }else if (parent->lc == node) {
            parent->lc = child;
        }else {
            parent->rc = child;
        }

        child->lc = node;
        node->parent = child;

        node->update();
        child->update();
    }
    void rotate_right(std::shared_ptr<Node> node) {
        auto child = node->lc;
        if (child == NIL) return;
        node->lc = child->rc;
        if (child->rc != NIL) child->rc->parent = node;

        auto parent = node->parent.lock();
        child->parent = parent;
        if (!parent || parent == NIL) {
            root = child;
            child->parent = NIL;
        }else if (parent->rc == node) {
            parent->rc = child;
        }else {
            parent->lc = child;
        }

        child->rc = node;
        node->parent = child;

        node->update();
        child->update();
    }

    void insert_fix(std::shared_ptr<Node> node) {
        while (node != root && node->get_parent()->color == Color::RED) {
            //node->color = Color::BLACK;

            auto parent = node->get_parent();
            auto grandparent = node->get_grandparent();
            auto uncle = node->get_uncle();

            //if (!parent || parent == NIL) break;
            //if (!grandparent || grandparent == NIL) break;

            if (grandparent->lc == parent) {     //parent is the left child of gp, rotation type is L_
                //auto uncle = grandparent->rc;
                if (uncle && uncle != NIL && uncle->color == Color::RED) {
                    uncle->switch_color();
                    parent->switch_color();
                    grandparent->switch_color();
                    node = grandparent;
                }else {
                    if (parent->rc == node) {                    //LR
                        node = parent;
                        rotate_left(node);
                        parent = node->get_parent();
                        grandparent = node->get_grandparent();
                    }
                    parent->switch_color();                      //LL
                    grandparent->switch_color();
                    node = grandparent;
                    rotate_right(node);
                    //parent is the initial node, which is black;
                }
            }else {                                              //parent is the right child of gp, rotation type is R_
                if (uncle && uncle != NIL && uncle->color == Color::RED) {
                    uncle->switch_color();
                    parent->switch_color();
                    grandparent->switch_color();
                    node = grandparent;
                }else {
                    if (parent->lc == node) {                     //RL
                        node = parent;
                        rotate_right(node);
                        parent = node->get_parent();
                        grandparent = node->get_grandparent();
                    }
                    parent->switch_color();                       //LL
                    grandparent->switch_color();
                    node = grandparent;
                    rotate_left(node);
                }
            }
        }
    }

    void fix_double_black(std::shared_ptr<Node> node) {
        if (node == NIL || node == root) return;
        if (node->color == Color::RED) {
            node->switch_color();
            return ;
        }
        auto parent = node->get_parent();
        auto sibling = node->get_sibling();

        //if (!parent || parent == NIL) return;
        //if (!sibling) return;

        if (sibling->color == Color::RED) {
            sibling->switch_color();
            parent->switch_color();
            if (parent->lc == node) {
                rotate_left(parent);
            }else {
                rotate_right(parent);
            }
            sibling = (parent->lc == node) ? parent->rc : parent->lc;
        }
        else if (sibling != NIL && sibling->rc != nullptr && sibling->lc != nullptr) {
            if (sibling->rc->color == Color::BLACK && sibling->lc->color == Color::BLACK) {
                sibling->switch_color();
                fix_double_black(parent);
            }else {
                if (parent->rc == node) {                      //L_ type
                    if (sibling->lc->color == Color::RED){     //LL type
                        sibling->lc->color = sibling->color;
                        sibling->color = parent->color;
                        parent->color = Color::BLACK;
                        rotate_right(parent);
                    }else {                                     //LR type
                        sibling->rc->color = parent->color;
                        parent->color = Color::BLACK;
                        rotate_left(sibling);
                        rotate_right(parent);
                    }
                }else {
                    if (sibling->rc->color == Color::RED){     //RR type
                        sibling->rc->color = sibling->color;
                        sibling->color = parent->color;
                        parent->color = Color::BLACK;
                        rotate_left(parent);
                    }else {                                     //RL type
                        sibling->lc->color = parent->color;
                        parent->color = Color::BLACK;
                        rotate_right(sibling);
                        rotate_left(parent);
                    }
                }
            }
        }
    }

    void update_upwards(std::shared_ptr<Node> curr) {
        while (curr && curr != NIL) {
            curr->update();
            curr = curr->get_parent();
        }
    }

    std::shared_ptr<Node> remove_in(std::shared_ptr<Node> &node, std::shared_ptr<Node> &p, Key val) {
        if (!node || node == NIL) return nullptr;
        std::shared_ptr<Node> res = nullptr;

        if (compare_(node->data.value(), val)) {
            res =  remove_in(node->rc, node, val);
        }else if (compare_(val, node->data.value())){
            res =  remove_in(node->lc, node, val);
        }else {
                if (node->rc == NIL && node->lc == NIL) {
                    std::shared_ptr<Node> curr = node;
                    //auto parent_to_update = node->get_parent();
                    if (node->color == Color::RED) {
                        curr->size = 0;
                        update_upwards(curr->get_parent());
                        node = NIL;
                        res = NIL;
                    }else {
                        fix_double_black(curr);
                        // 通过 curr 自己找回真实的父亲，切断联系
                        auto real_parent = curr->get_parent();
                        if (real_parent) {
                            if (real_parent->lc == curr) real_parent->lc = NIL;
                            else real_parent->rc = NIL;
                        } else {
                            root = NIL; // 如果它是根节点
                        }
                        res = NIL;
                        update_upwards(real_parent);
                    }

                }else if (node->rc == NIL || node->lc == NIL) {
                    auto parent = node->get_parent();
                    node = std::move(node->rc == NIL ? node->lc : node->rc);
                    if (node && node != NIL) node->parent = parent;
                    node->color = Color::BLACK;
                    res = node;
                    update_upwards(parent);
                }else {
                    auto successor = node->rc;
                    while (successor->lc != NIL) successor = successor->lc;

                    node->data = successor->data;
                    res = remove_in(node->rc, node, node->data.value());
                }
        }

        return res;
    }

    std::shared_ptr<Node> search_in(std::shared_ptr<Node> node, const Key &val) const{
        if (!node || node == NIL) return NIL;
        if (compare_(node->data.value(), val)) return search_in(node->rc, val);
        if (compare_(val, node->data.value())) return search_in(node->lc, val);
        return node;
    }

    int get_rank_in(std::shared_ptr<Node> n, const Key& val) const{
        if (!n || n == NIL) return 0;

        if (compare_(n->data.value(), val)) {
            return (n->lc != NIL ? n->lc->size : 0) + 1 + get_rank_in(n->rc, val);
        } else if (compare_(val, n->data.value())) {
            return get_rank_in(n->lc, val);
        } else {
            return (n->lc != NIL ? n->lc->size : 0) + 1;
        }
    }
    int get_rank_less_than(std::shared_ptr<Node> n, const Key& val) const{
        if (!n || n == NIL) return 0;

        if (compare_(n->data.value(), val)) {
            return (n->lc != NIL ? n->lc->size : 0) + 1 + get_rank_less_than(n->rc, val);
        } else {
            return get_rank_less_than(n->lc, val);
        }
    }

public:
    ESet() {
        init_NIL();
        root = NIL;
    }
    ~ESet() {
        clear();
    }

    std::shared_ptr<Node> find_first_node(const ESet* cur_set) const{
        auto tar = cur_set->root;
        while (tar != NIL && tar->lc != NIL) {
            tar = tar->lc;
        }
        return tar;
    }
    std::shared_ptr<Node> find_last_node(const ESet* cur_set) const{
        auto tar = cur_set->root;
        while (tar != NIL && tar->rc != NIL) {
            tar = tar->rc;
        }
        return tar;
    }

    template <bool IsConst>
    class IteratorImpl {
    public:
        using ValueType = std::conditional_t<IsConst, const Key, Key>;
        using SetPtr = std::conditional_t<IsConst, const ESet*, ESet*>;
        using Pointer   = ValueType*;
        using Reference = ValueType&;
        template <bool> friend class IteratorImpl;

    private:
        std::shared_ptr<Node> cur_node;
        //std::shared_ptr<Node> NIL;
        const ESet* cur_set;
        friend class ESet;

    public:
        IteratorImpl(): cur_node(nullptr), cur_set(nullptr){}
        IteratorImpl(const std::shared_ptr<Node> cur_node_, const ESet* cur_set_)
        : cur_node(cur_node_), cur_set(cur_set_){}
        IteratorImpl(const IteratorImpl& other): cur_node(other.cur_node), cur_set(other.cur_set) {}
        template <bool OtherIsConst>
        IteratorImpl(const IteratorImpl<OtherIsConst>& other, std::enable_if_t<IsConst && !OtherIsConst>* = nullptr)
            : cur_node(other.cur_node), cur_set(other.cur_set) {}
        ~IteratorImpl() = default;

        template <bool OtherIsConst>
        bool operator==(const IteratorImpl<OtherIsConst>& other) const {
            if (!cur_set || !other.cur_set) throw std::runtime_error("invalid iterator");
            return this->cur_set == other.cur_set && this->cur_node == other.cur_node;
        }

        template <bool OtherIsConst>
        bool operator!=(const IteratorImpl<OtherIsConst>& other) const {
            return !operator==(other);
        }

        IteratorImpl &operator++() {
            if (!cur_set) throw std::runtime_error("invalid iterator");
            if (this->cur_node != cur_set->NIL) {
                if (this->cur_node->rc != cur_set->NIL) {
                    auto tar = this->cur_node->rc;
                    while (tar->lc != cur_set->NIL) tar = tar->lc;
                    this->cur_node = tar;
                }
                else {
                    auto tar_p = this->cur_node->get_parent();
                    auto tmp = cur_node;
                    while (tar_p && tar_p != cur_set->NIL && tar_p->lc != tmp) {
                        if (tar_p->get_parent() == cur_set->NIL) {
                            this->cur_node = cur_set->NIL;
                            return *this;
                        }
                        tmp = tar_p;
                        tar_p = tar_p->get_parent();
                    }
                    this->cur_node = tar_p;
                }
            }
            return *this;
        }
        IteratorImpl operator++(int) {
            auto tmp = *this;
            this->operator++();
            return tmp;
        }

        IteratorImpl &operator--() {
            if (!cur_set) throw std::runtime_error("invalid iterator");
            if (this->cur_node == cur_set->NIL) {
                auto last_node = cur_set->find_last_node(cur_set);
                this->cur_node = last_node;
                return *this;
            }
            auto first_node = cur_set->find_first_node(cur_set);
            if (this->cur_node != first_node) {
                if (this->cur_node->lc != cur_set->NIL) {
                    auto tar = this->cur_node->lc;
                    while (tar->rc != cur_set->NIL) tar = tar->rc;
                    this->cur_node = tar;
                }
                else {
                    auto tar_p = this->cur_node->get_parent();
                    auto tmp = cur_node;
                    while (tar_p && tar_p != cur_set->NIL && tar_p->rc != tmp) {
                        tmp = tar_p;
                        tar_p = tar_p->get_parent();
                    }
                    this->cur_node = tar_p;
                }
            }
            return *this;
        }

        IteratorImpl operator--(int) {
            auto tmp = *this;
            this->operator--();
            return tmp;
        }

        Reference operator*() const {
            //throw std::runtime_error("invalid operation for iterator : *");
            if (!cur_set || cur_node == cur_set->NIL) {
                throw std::runtime_error("invalid iterator");
            }
            return cur_node->data.value();
        }
        Pointer operator->() const {
            //throw std::runtime_error("invalid operation for iterator : ->");
            if (!cur_set || cur_node == cur_set->NIL) {
                throw std::runtime_error("invalid iterator");
            }
            return &cur_node->data.value();
        }

    };
    using iterator = IteratorImpl<false>;
    using const_iterator = IteratorImpl<true>;

    iterator begin() {
        if (root == NIL) return end();
        return iterator(find_first_node(this), this);
    }
    iterator end() {
        return iterator(NIL, this);
    }
    const_iterator begin() const{
        return const_iterator(find_first_node(this), this);
    }
    const_iterator end() const{
        return const_iterator(NIL, this);
    }

    std::pair<iterator,bool> insert(Key val) {
        auto new_node = insert_in(root, NIL, val);
        insert_fix(new_node.first);
        root->color = Color::BLACK;
        return {iterator(new_node.first, this),new_node.second};
    }
    template< class... Args >
    std::pair<iterator, bool> emplace( Args&&... args ) {
        return insert(Key(std::forward<Args>(args)...));
    }
    size_t erase(Key key) {
        return remove_in(root, NIL, key) != nullptr ? 1 : 0;
    }

    iterator find( const Key& key ) const {
        return iterator(search_in(root, key), this);
    }

    void clear() {
        // 暂时断开 root 之前，先保护好 NIL，防止它被意外销毁
        if (!root || root == NIL) return;

        std::shared_ptr<Node> curr = root;
        root = NIL; // 确保 ESet 不再持有树的引用

        while (curr && curr != NIL) {
            if (node_has_left_child(curr)) {
                // 如果有左孩子，进行一次“右旋”操作，把左孩子转到右边去
                auto left_child = curr->lc;
                curr->lc = left_child->rc;
                left_child->rc = curr;
                curr = left_child;
            } else {
                // 如果没有左孩子了，直接销毁当前节点，移动到右孩子
                auto next_node = curr->rc;
                // 手动切断连接，防止递归
                curr->lc = nullptr;
                curr->rc = nullptr;
                curr = next_node;
            }
        }
    }

    // 辅助函数判断
    bool node_has_left_child(const std::shared_ptr<Node>& n) {
        return n->lc && n->lc != NIL;
    }


    ESet(const ESet& other) : ESet() {
        for (const auto& item : other) {
            this->insert(item);
        }
    }

    ESet& operator=(const ESet& other) {
        if (this != &other) {
            this->clear();
            for (const auto& item : other) {
                this->insert(item);
            }
        }
        return *this;
    }

    ESet(ESet&& other) noexcept
    : root(std::move(other.root)), NIL(std::move(other.NIL)) {
        other.init_NIL();
        other.root = other.NIL;
    }

    ESet& operator=(ESet&& other) noexcept {
        if (this != &other) {
            this->clear();
            this->root = std::move(other.root);
            this->NIL = std::move(other.NIL);
            other.init_NIL();
            other.root = NIL;
        }
        return *this;
    }

    [[nodiscard]] size_t size() const noexcept {
        return root->size;
    }

    iterator lower_bound(const Key& key) const {
        auto curr = root;
        auto res = NIL;

        while (curr != NIL) {
            if (!compare_(curr->data.value(), key)) {
                // cur >= key
                res = curr;
                curr = curr->lc;
            } else {
                curr = curr->rc;
            }
        }
        return iterator(res, this);
    }
    iterator upper_bound(const Key& key) const {
        auto curr = root;
        auto res = NIL;

        while (curr != NIL) {
            if (compare_(key, curr->data.value())) {
                // cur > key
                res = curr;
                curr = curr->lc;
            } else {
                curr = curr->rc;
            }
        }
        return iterator(res, this);
    }

    int count_real(std::shared_ptr<Node> n, const Key& l, const Key& r) const {   //debug
        if (n == NIL) return 0;
        int cnt = 0;
        if (!compare_(n->data.value(), l) && !compare_(r, n->data.value())) cnt = 1; // l <= val <= r
        return cnt + count_real(n->lc, l, r) + count_real(n->rc, l, r);
    }
    void check_entire_tree(std::shared_ptr<Node> n) const {
        if (n == NIL) return;
        int expected = (n->lc != NIL ? n->lc->size : 0) + (n->rc != NIL ? n->rc->size : 0) + 1;
        if (n->size != expected) {
            std::cerr << "发现僵尸节点,值: " << n->data.value()
                      << " 记录 size: " << n->size << " 实际应为: " << expected << std::endl;
            abort();
        }
        check_entire_tree(n->lc);
        check_entire_tree(n->rc);
    }

    size_t range( const Key& l, const Key& r ) const {
        //check_entire_tree(root);
        if (compare_(r, l)) return 0;
        return get_rank_in(root, r) - get_rank_less_than(root, l);
        size_t fast = get_rank_in(root, r) - get_rank_less_than(root, l);  //debug
        size_t slow = count_real(root, l, r);
        if (fast != slow) {
            std::cerr << "Size Inconsistency Detected!" <<fast<<" vs "<<slow<< std::endl;
        }
        return fast;
    }

};
#endif //ESET_ESET_HPP

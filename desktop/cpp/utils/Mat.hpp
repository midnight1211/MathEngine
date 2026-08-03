#pragma once
// utils/Mat.hpp
// =============================================================================
// Mat<T> — a 2-D matrix with row-major contiguous storage.
// Replaces every Vec<Vec<T>> / std::vector<std::vector<T>> usage:
//   • DMat / Matrix / RMat / CMat / IMat / FMatrix / ExprMat
//
// Two usage patterns coexist in the codebase:
//   A) Jagged matrices (rows of different lengths) — supported by construction
//      from Vec<Vec<T>>.
//   B) Dense rectangular matrices — the primary case; use rows()/cols().
//
// The matrix exposes the same row-based operator[] as a Vec<Vec<T>> would,
// so existing code like M[i][j] and "for (auto& row : M)" just works.
// =============================================================================

#include "Vec.hpp"
#include <cstddef>
#include <stdexcept>
#include <initializer_list>
#include <sstream>

namespace utils {

template<typename T>
class Mat {
public:
    using value_type      = Vec<T>;
    using size_type       = std::size_t;
    using reference       = Vec<T>&;
    using const_reference = const Vec<T>&;
    using iterator        = typename Vec<Vec<T>>::iterator;
    using const_iterator  = typename Vec<Vec<T>>::const_iterator;

    // ── Constructors ──────────────────────────────────────────────────────────
    Mat() = default;

    // rows × cols matrix filled with val
    Mat(size_type rows, size_type cols, const T& val = T())
        : rows_(rows), cols_(cols), data_(rows, Vec<T>(cols, val))
    {}

    // Construct from Vec<Vec<T>> (jagged OK; cols_ = max row length)
    explicit Mat(Vec<Vec<T>> data)
        : data_(std::move(data))
    {
        rows_ = data_.size();
        cols_ = 0;
        for (auto& row : data_)
            if (row.size() > cols_) cols_ = row.size();
    }

    // Initializer-list of initializer-lists
    Mat(std::initializer_list<std::initializer_list<T>> il)
        : rows_(il.size()), cols_(0)
    {
        for (auto& row : il) {
            data_.push_back(Vec<T>(row));
            if (data_.back().size() > cols_) cols_ = data_.back().size();
        }
    }

    // Copy / move — defaulted (Vec handles it)
    Mat(const Mat&)            = default;
    Mat& operator=(const Mat&) = default;
    Mat(Mat&&)                 = default;
    Mat& operator=(Mat&&)      = default;

    // ── Dimensions ────────────────────────────────────────────────────────────
    size_type rows() const noexcept { return rows_; }
    size_type cols() const noexcept { return cols_; }
    size_type size() const noexcept { return rows_; }   // row count (like vector)
    bool      empty()const noexcept { return rows_ == 0; }

    // ── Row access ────────────────────────────────────────────────────────────
    reference       operator[](size_type i)       noexcept { return data_[i]; }
    const_reference operator[](size_type i) const noexcept { return data_[i]; }

    reference at(size_type i)
    {
        if (i >= rows_) throw std::out_of_range("Mat::at row out of range");
        return data_[i];
    }
    const_reference at(size_type i) const
    {
        if (i >= rows_) throw std::out_of_range("Mat::at row out of range");
        return data_[i];
    }

    // Element access by (row, col)
    T& at(size_type r, size_type c)
    {
        if (r >= rows_ || c >= data_[r].size())
            throw std::out_of_range("Mat::at(r,c) out of range");
        return data_[r][c];
    }
    const T& at(size_type r, size_type c) const
    {
        if (r >= rows_ || c >= data_[r].size())
            throw std::out_of_range("Mat::at(r,c) out of range");
        return data_[r][c];
    }

    // ── Iterators (iterate over rows) ─────────────────────────────────────────
    iterator       begin()        noexcept { return data_.begin(); }
    const_iterator begin()  const noexcept { return data_.begin(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }
    iterator       end()          noexcept { return data_.end(); }
    const_iterator end()    const noexcept { return data_.end(); }
    const_iterator cend()   const noexcept { return data_.cend(); }

    // ── Modifiers ─────────────────────────────────────────────────────────────
    void push_back(Vec<T> row)
    {
        if (row.size() > cols_) cols_ = row.size();
        data_.push_back(std::move(row));
        rows_ = data_.size();
    }

    void clear()
    {
        data_.clear();
        rows_ = cols_ = 0;
    }

    // Swap two rows
    void swap_rows(size_type a, size_type b)
    {
        if (a != b) {
            Vec<T> tmp = std::move(data_[a]);
            data_[a]   = std::move(data_[b]);
            data_[b]   = std::move(tmp);
        }
    }

    // ── Comparison ────────────────────────────────────────────────────────────
    bool operator==(const Mat& o) const { return data_ == o.data_; }
    bool operator!=(const Mat& o) const { return !(*this == o); }

    // ── Conversion to Vec<Vec<T>> for interop ─────────────────────────────────
    Vec<Vec<T>>& raw()             { return data_; }
    const Vec<Vec<T>>& raw() const { return data_; }

    // ── Transpose ─────────────────────────────────────────────────────────────
    Mat transpose() const
    {
        if (rows_ == 0 || cols_ == 0) return Mat();
        Mat result(cols_, rows_);
        for (size_type r = 0; r < rows_; ++r)
            for (size_type c = 0; c < data_[r].size(); ++c)
                result[c][r] = data_[r][c];
        return result;
    }

    // ── Identity matrix factory ───────────────────────────────────────────────
    static Mat identity(size_type n, const T& one = T(1), const T& zero = T())
    {
        Mat I(n, n, zero);
        for (size_type i = 0; i < n; ++i) I[i][i] = one;
        return I;
    }

    // ── Debug string ─────────────────────────────────────────────────────────
    std::string to_string() const
    {
        std::ostringstream ss;
        ss << "[";
        for (size_type r = 0; r < rows_; ++r) {
            ss << "[";
            for (size_type c = 0; c < data_[r].size(); ++c) {
                if (c) ss << ",";
                ss << data_[r][c];
            }
            ss << "]";
            if (r + 1 < rows_) ss << ",";
        }
        ss << "]";
        return ss.str();
    }

private:
    Vec<Vec<T>> data_;
    size_type   rows_ = 0;
    size_type   cols_ = 0;
};

// Convenience aliases
using DMat  = Mat<double>;
using IMat  = Mat<long long>;
using SMat  = Mat<std::string>;

} // namespace utils

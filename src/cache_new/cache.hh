#pragma once

template <typename KEY, typename VALUE>
class Cache {
  private: 
    size_t max_;
 
  public:
    Cache(int);

    virtual VALUE& lookup (KEY) = 0;
    virtual bool exists (KEY) = 0;
    virtual void insert (KEY, VALUE) = 0;

    size_t get_max () const;
    Cache& set_max (size_t);
};

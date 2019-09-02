#ifndef CLargestRect_H
#define CLargestRect_H

// Template class to extract largest rectange from array of values DATA of type VALUE
//
// DATA must support the getValue(int ix, int iy) method.
template<typename DATA, typename VALUE>
class CLargestRect {
 public:
  struct Rect {
    int left { 0 }, top { 0 }, width { -1 }, height { -1 };

    Rect(int l=0, int t=0, int w=-1, int h=-1) :
     left(l), top(t), width(w), height(h) {
    }

    bool isValid() const { return (width > 0 && height > 0); }

    int area() const {
      return width*height;
    }

    friend bool operator==(const Rect &r1, const Rect &r2) {
      return (r1.left  == r2.left  && r1.top    == r2.top    &&
              r1.width == r2.width && r1.height == r2.height);
    }

    friend std::ostream &operator<<(std::ostream &os, const Rect &r) {
      os << r.left << " " << r.top << " " << r.width << " " << r.height;
      return os;
    }
  };

  struct Point {
    int x, y;

    Point(int x1=0, int y1=0) :
     x(x1), y(y1) {
    }

    friend bool operator==(const Point &p1, const Point &p2) {
      return (p1.x == p2.x && p1.y == p2.y);
    }

    friend std::ostream &operator<<(std::ostream &os, const Point &p) {
      os << p.x << " " << p.y;
      return os;
    }
  };

 public:
  CLargestRect(const DATA &data, int width, int height) :
   data_(data), width_(width), height_(height) {
  }

  Rect largestRect(const VALUE &value) const {
    Point best_ll( 0,  0);
    Point best_ur(-1, -1); // bug fix, was 0,0

    std::vector<int> c;

    c.resize(width_ + 1);

    int width1, x0, w0;

    typedef std::pair<int,int> Xw;

    Xw xw;

    std::vector<Xw> s;

    for (int y = 0; y < height_; ++y) {
      updateCache(y, c, value);

      width1 = 0;

      for (int x = 0; x < width_ + 1; ++x) {
        if      (c[x] > width1) {
          s.push_back(Xw(x, width1));

          width1 = c[x];
        }
        else if (c[x] < width1) {
          do {
            xw = s.back(); s.pop_back();

            x0 = xw.first;
            w0 = xw.second;

            if (width1 * (x - x0) > area(best_ll, best_ur)) {
              best_ll = Point(x0, y - width1 + 1);
              best_ur = Point(x - 1, y);
            }

            width1 = w0;
          } while (c[x] < width1);

          width1 = c[x];

          if (width1 != 0)
            s.push_back(Xw(x0, w0));
        }
      }
    }

    return rectFromPoints(best_ll, best_ur);
  }

 private:
  void updateCache(int y, std::vector<int> &c, const VALUE &value) const {
    for (int x = 0; x < width_; ++x) {
      if (data_.getValue(x, y) == value)
        c[x]++;
      else
        c[x] = 0;
    }
  }

  int area(const Point &ll, const Point &ur) const {
    if (ll.x > ur.x || ll.y > ur.y)
      return 0;
    else
      return (ur.x - ll.x + 1) * (ur.y - ll.y + 1);
  }

  Rect rectFromPoints(const Point &ll, const Point &ur) const {
    return Rect(ll.x, ll.y, ur.x - ll.x + 1, ur.y - ll.y + 1);
  }

 private:
  const DATA &data_;
  int         width_ { 0 }, height_ { 0 };
};

#endif

class Base {
    int m_bx;
    protected:
        int m_y;
};

class C: public Base {
    int m_x = 2;
    int *m_ptr = nullptr;

    public:
        // should warn
        auto f0() {
            auto g = [=]() {
                return m_x;
            };
            return g;
        }
        auto f1() {
            int cap;
            auto g = [&cap] {
                return cap;
            };
            return g;
        }
        auto f2() {
            int *m = nullptr;
            auto g = [m]() {
            };
            return g;
        }
        auto f3() {
            int &v = m_x;
            auto g = [&]() {
                return v;
            };
            return g;
        }
        auto f4() {
            auto p = m_ptr;
            auto g = [=]() {
                return *p;
            };
            return g;
        }

        // should not warn
        auto t0() {
            int &v = m_x;
            auto h = [=]() {
                const int *p = &v;
                return p;
            };
            return h;
        }
        auto t1() {
            int n = m_x;
            auto h = [n]() {
                return &n;
            };
            return h;
        }
};

template<class T>
class TestUnresolved {
    int *m_ptr;

    public:
        auto f() {
            auto dependent = m_ptr;
            auto g = [=]() {
                return dependent;
            };
            return g;
        }
};

void use_unresolve() {
    TestUnresolved<void>{}.f();
}


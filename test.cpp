class C {
    int m_x = 2;

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

        // should not warn
        auto t0() {
            int &v = m_x;
            auto g = [=]() {
                return v;
            };
            return g;
        }
};


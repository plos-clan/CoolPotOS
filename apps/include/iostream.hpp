#pragma once

namespace std {
    class iostream_cout {
    public:
        const iostream_cout &operator<<(int value) const;
        const iostream_cout &operator<<(char *str) const;
    };
}


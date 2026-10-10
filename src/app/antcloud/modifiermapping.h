// AntCloud Native additions, GPL-3.0-or-later, 2026.
#pragma once
#include <array>

namespace AntCloud {
// Rows: Command, Option, Control. Targets: None, Ctrl, Alt, Windows.
class ModifierMapping {
public:
    ModifierMapping() = default;
    explicit ModifierMapping(std::array<int, 3> values) : m_Values(values) {
        for (int row = 0; row < 3; ++row) {
            if (m_Values[row] < 0 || m_Values[row] > 3) m_Values[row] = 0;
            for (int earlier = 0; earlier < row; ++earlier)
                if (m_Values[row] != 0 && m_Values[row] == m_Values[earlier]) m_Values[row] = 0;
        }
    }
    int target(int row) const { return row >= 0 && row < 3 ? m_Values[row] : 0; }
    bool assign(int row, int target) {
        if (row < 0 || row >= 3 || target < 0 || target > 3 || m_Values[row] == target) return false;
        for (int other = 0; other < 3; ++other)
            if (other != row && target != 0 && m_Values[other] == target) m_Values[other] = 0;
        m_Values[row] = target;
        return true;
    }
    unsigned packed() const { return unsigned(m_Values[0] | (m_Values[1] << 2) | (m_Values[2] << 4)); }
    static ModifierMapping fromPacked(unsigned value) {
        return ModifierMapping({{int(value & 3), int((value >> 2) & 3), int((value >> 4) & 3)}});
    }
private:
    std::array<int, 3> m_Values{{1, 3, 2}};
};
}

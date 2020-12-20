#include "blocks.h"
#include "error.h"

Blocks::Blocks(size_t n_blocks) :
    m_n_done(0),
    m_states(n_blocks, EMPTY) {}

void Blocks::set_state(int blk_id, int state) {
    is_valid(blk_id);

    if (state == DONE && m_states[blk_id] != DONE)
        m_n_done++;
    m_states[blk_id] = state;
}

int Blocks::get_state(int blk_id) {
    is_valid(blk_id);

    return m_states[blk_id];
}

void Blocks::append_states(const std::vector<int> &states) {
    m_states.insert(m_states.end(), states.begin(), states.end());
    for (int state: states) {
        if (state == DONE)
            m_n_done++;
    }
}

void Blocks::set_n_blocks(int n_blocks, int state) {
    if (state == DONE)
        m_n_done = n_blocks;
    else
        m_n_done = 0;
    m_states.assign(n_blocks, state);
}

int Blocks::get_n_blocks() const {
    return m_states.size();
}

void Blocks::reset() {
    size_t size = m_states.size();
    m_states.assign(size, EMPTY);
    m_n_done = 0;
}

bool Blocks::finished() const {
    return m_n_done == m_states.size();
}

void Blocks::is_valid(int blk_id) {
    if (blk_id < 0 || (size_t)blk_id >= m_states.size())
        throw PE("Invalid block id");
}

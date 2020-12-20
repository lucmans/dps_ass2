#ifndef __BLOCKS_H__
#define __BLOCKS_H__

#include "consts.h"

#include <vector>

const int EMPTY         = 0;
const int DOWNLOADING   = 1;
const int DONE          = 2;

inline int size_to_nblks(size_t size) {
    return (size + PREON_BLOCK_SIZE - 1) / PREON_BLOCK_SIZE;
}

class Blocks {
    public:
        Blocks(size_t n_blocks = 0);

        void set_state(int blk_id, int state);
        int get_state(int blk_id);
        void append_states(const std::vector<int> &states);

        void set_n_blocks(int n_blocks, int state);
        int get_n_blocks() const;

        void reset();

        bool finished() const;

        std::vector<int>::iterator begin() {return m_states.begin();}
        std::vector<int>::iterator end() {return m_states.end();}

    private:
        size_t m_n_done;
        std::vector<int> m_states;

        void is_valid(int blk_id);
};

#endif  //#ifndef __BLOCKS_H__

#include "rdtree_node.hpp"

int RDtreeNode::get_size() const
{
  return read_uint16(&data.data()[2]);
}

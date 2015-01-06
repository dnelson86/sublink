#pragma once
/** @file TreeUtil.hpp
 * @brief Useful functions for working with Subfind catalogs and
 *        SubLink merger trees.
 *
 * @author Vicente Rodriguez-Gomez (vrodriguez-gomez@cfa.harvard.edu)
 */

#include <iostream>
#include <vector>

#include "../InputOutput/ReadTreeHDF5.hpp"

/** Get some types from ReadTreeHDF5.hpp and make them our own. */
typedef Tree::Snapshot Snapshot;
typedef Tree::Subhalo Subhalo;
typedef typename Tree::sub_id_type sub_id_type;
typedef typename Tree::index_type index_type;
typedef typename Tree::snapnum_type snapnum_type;

/** @brief Function to calculate subhalo offsets.
 *
 * In this context, the subhalo offset is the index of the
 * first particle of a given type that belongs to each subhalo.
 */
std::vector<uint32_t> calculate_subhalo_offsets(const std::string& basedir,
    const int16_t snapnum, const int parttype) {

  // Load some FoF group and subhalo info
  auto group_nsubs = subfind::read_block<uint32_t>(basedir, snapnum, "Group",
      "GroupNsubs", -1);
  auto ngroups = subfind::get_scalar_attribute<uint32_t>(basedir, snapnum,
      "Ngroups_Total");
  auto nsubs = subfind::get_scalar_attribute<uint32_t>(basedir, snapnum,
      "Nsubgroups_Total");
  auto group_len = subfind::read_block<uint32_t>(basedir, snapnum, "Group",
      "GroupLenType", parttype);
  auto sub_len = subfind::read_block<uint32_t>(basedir, snapnum, "Subhalo",
      "SubhaloLenType", parttype);
  std::vector<uint32_t> group_offset(ngroups, 0);
  std::vector<uint32_t> sub_offset(nsubs, 0);

  // Calculate offsets
  uint32_t k = 0;
  for (uint32_t i = 0; i < ngroups; ++i) {
    if (i>0)
      group_offset[i] = group_offset[i-1] + group_len[i-1];
    if (group_nsubs[i] > 0) {
      sub_offset[k] = group_offset[i];
      ++k;
      for (uint32_t j = 1; j < group_nsubs[i]; ++j) {
        sub_offset[k] = sub_offset[k-1] + sub_len[k-1];
        ++k;
      }
    }
  }
  // Sanity check
  if (k != nsubs)
    std::cerr << "Problem with subhalo offsets: " << k << " not equal to " <<
        nsubs << std::endl;

  return sub_offset;
}

/** @brief Get the progenitor along the main branch at a given snapshot.
 * @param[in] sub The Subhalo of interest.
 * @param[in] snapnum The snapshot number of the progenitor of interest.
 * @pre @a sub is valid.
 * @pre @a snapnum < @a sub.snapnum()
 * @return The latest progenitor along the main branch of @a sub such that
 *         @a result.snapnum() <= @a snapnum, or an invalid Subhalo if
 *         the main branch is truncated.
 */
Subhalo back_in_time(Subhalo sub, snapnum_type snapnum) {
  assert((sub.is_valid()) && (sub.snapnum() >= snapnum));
  while ((snapnum < sub.snapnum()) && (sub.is_valid()))
    sub = sub.first_progenitor();
  return sub;
}

/** @brief Get the progenitors of @a primary and @a secondary at
 *         the moment of infall or earlier.
 * @param[in] primary The primary subhalo.
 * @param[in] secondary The secondary subhalo.
 * @return A pair with the main branch progenitors of @a primary
 *         and @a secondary at the latest snapshot such that
 *         @a primary and @a secondary do not belong to the same
 *         FoF group. Return a pair with invalid subhalos if one
 *         or both branches are truncated.
 *
 * @pre @a primary and @a secondary are valid subhalos.
 * @pre @a secondary.snapnum() <= @a primary.snapnum()
 * @post new(@a primary).snapnum() == new(@a secondary).snapnum()
 */
std::pair<Subhalo, Subhalo> infall_pair(Subhalo primary,
    Subhalo secondary) {
  assert(primary.is_valid() && secondary.is_valid());
  assert(secondary.snapnum() <= primary.snapnum());

  while (true) {
    // If secondary branch is truncated, return invalid Subhalos.
    if (!secondary.is_valid())
      return std::make_pair(Subhalo(), Subhalo());

    // If primary branch is truncated, return invalid Subhalos.
    auto cur_snapnum = secondary.snapnum();
    primary = back_in_time(primary, cur_snapnum);
    if (!primary.is_valid())
      return std::make_pair(Subhalo(), Subhalo());

    // If primary skipped this snapshot, "increment" secondary and try again.
    if (primary.snapnum() != cur_snapnum) {
      secondary = secondary.first_progenitor();
      continue;
    }
    // If we got here, both primary and secondary are valid Subhalos at
    // the same snapshot. If they do not belong to the same FoF group,
    // we have reached infall.
    if (  primary.first_subhalo_in_fof_group() !=
        secondary.first_subhalo_in_fof_group())
      return std::make_pair(primary, secondary);
  }
  assert(false);
}

/** @brief Get the progenitors of @a primary and @a secondary at
 *         the same snapshot as @a secondary or earlier.
 * @param[in] primary The primary subhalo.
 * @param[in] secondary The secondary subhalo.
 * @return A pair with the main branch progenitors of @a primary
 *         and @a secondary at the latest snapshot @a snapnum
 *         such that @a snapnum <= @a secondary.snapnum().
 *         Return a pair with invalid subhalos if one or both
 *         branches are truncated.
 *
 * @pre @a primary and @a secondary are valid subhalos.
 * @pre @a secondary.snapnum() <= @a primary.snapnum()
 * @post new(@a primary).snapnum() == new(@a secondary).snapnum()
 *
 * @note Implementation of this function is very similar to infall_pair().
 *       Both functions could rely on a more general function in which the two
 *       subhalos are "moved" back to a moment when a certain condition
 *       becomes true.
 */
std::pair<Subhalo, Subhalo> synchronize_subhalos(Subhalo primary,
    Subhalo secondary) {
  assert(primary.is_valid() && secondary.is_valid());
  assert(secondary.snapnum() <= primary.snapnum());

  while (true) {
    // If secondary branch is truncated, return invalid Subhalos.
    if (!secondary.is_valid())
      return std::make_pair(Subhalo(), Subhalo());

    // If primary branch is truncated, return invalid Subhalos.
    auto cur_snapnum = secondary.snapnum();
    primary = back_in_time(primary, cur_snapnum);
    if (!primary.is_valid())
      return std::make_pair(Subhalo(), Subhalo());

    // If primary skipped this snapshot, "increment" secondary and try again.
    if (primary.snapnum() != cur_snapnum) {
      secondary = secondary.first_progenitor();
      continue;
    }
    // If we got here, both primary and secondary are valid Subhalos at
    // the same snapshot.
    return std::make_pair(primary, secondary);
  }
  assert(false);
}

/** @brief Return true if @a secondary has already "infalled" into
 *         the same FoF group as @a primary; false otherwise.
 * @pre @a primary and @a secondary are valid subhalos.
 * @pre @a secondary.snapnum() <= @a primary.snapnum()
 */
bool after_infall(Subhalo primary, Subhalo secondary) {
  auto synced_pair = synchronize_subhalos(primary, secondary);
  primary = synced_pair.first;
  secondary = synced_pair.second;
  if (!primary.is_valid() || !secondary.is_valid())
    return false;
  assert(primary.first_subhalo_in_fof_group().is_valid());
  return (primary.first_subhalo_in_fof_group() ==
      secondary.first_subhalo_in_fof_group());
}

/** @brief Return true if @a prog lies along the main branch of @a sub.
 * @pre @a sub and @a prog are valid subhalos.
 */
bool along_main_branch(Subhalo sub, Subhalo prog) {
  return (prog.data().SubhaloID >= sub.data().SubhaloID) &&
         (prog.data().SubhaloID <= sub.data().MainLeafProgenitorID);
}
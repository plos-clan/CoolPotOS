# lib/libfdt/fdt_overlay.c

## `static uint32_t overlay_get_target_phandle(const void *fdto, int fragment) {`


overlay_get_target_phandle - retrieves the target phandle of a fragment
@fdto: pointer to the device tree overlay blob
@fragment: node offset of the fragment in the overlay

overlay_get_target_phandle() retrieves the target phandle of an
overlay fragment when that fragment uses a phandle (target
property) instead of a path (target-path property).

returns:
the phandle pointed by the target property
0, if the phandle was not found
-1, if the phandle was malformed


---

## `static int overlay_phandle_add_offset(void *fdt, int node, const char *name, uint32_t delta) {`


overlay_phandle_add_offset - Increases a phandle by an offset
@fdt: Base device tree blob
@node: Device tree overlay blob
@name: Name of the property to modify (phandle or linux,phandle)
@delta: offset to apply

overlay_phandle_add_offset() increments a node phandle by a given
offset.

returns:
0 on success.
Negative error code on error


---

## `static int overlay_adjust_node_phandles(void *fdto, int node, uint32_t delta) {`


overlay_adjust_node_phandles - Offsets the phandles of a node
@fdto: Device tree overlay blob
@node: Offset of the node we want to adjust
@delta: Offset to shift the phandles of

overlay_adjust_node_phandles() adds a constant to all the phandles
of a given node. This is mainly use as part of the overlay
application process, when we want to update all the overlay
phandles to not conflict with the overlays of the base device tree.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_adjust_local_phandles(void *fdto, uint32_t delta) {`


overlay_adjust_local_phandles - Adjust the phandles of a whole overlay
@fdto: Device tree overlay blob
@delta: Offset to shift the phandles of

overlay_adjust_local_phandles() adds a constant to all the
phandles of an overlay. This is mainly use as part of the overlay
application process, when we want to update all the overlay
phandles to not conflict with the overlays of the base device tree.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_update_local_node_references(void *fdto, int tree_node, int fixup_node, uint32_t delta) {`


overlay_update_local_node_references - Adjust the overlay references
@fdto: Device tree overlay blob
@tree_node: Node offset of the node to operate on
@fixup_node: Node offset of the matching local fixups node
@delta: Offset to shift the phandles of

overlay_update_local_nodes_references() update the phandles
pointing to a node within the device tree overlay by adding a
constant delta.

This is mainly used as part of a device tree application process,
where you want the device tree overlays phandles to not conflict
with the ones from the base device tree before merging them.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_update_local_references(void *fdto, uint32_t delta) {`


overlay_update_local_references - Adjust the overlay references
@fdto: Device tree overlay blob
@delta: Offset to shift the phandles of

overlay_update_local_references() update all the phandles pointing
to a node within the device tree overlay by adding a constant
delta to not conflict with the base overlay.

This is mainly used as part of a device tree application process,
where you want the device tree overlays phandles to not conflict
with the ones from the base device tree before merging them.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_fixup_one_phandle(void *fdt, void *fdto, int symbols_off, const char *path, uint32_t path_len, const char *name, uint32_t name_len, int poffset, uint32_t phandle) {`


overlay_fixup_one_phandle - Set an overlay phandle to the base one
@fdt: Base Device Tree blob
@fdto: Device tree overlay blob
@symbols_off: Node offset of the symbols node in the base device tree
@path: Path to a node holding a phandle in the overlay
@path_len: number of path characters to consider
@name: Name of the property holding the phandle reference in the overlay
@name_len: number of name characters to consider
@poffset: Offset within the overlay property where the phandle is stored
@phandle: Phandle referencing the node

overlay_fixup_one_phandle() resolves an overlay phandle pointing to
a node in the base device tree.

This is part of the device tree overlay application process, when
you want all the phandles in the overlay to point to the actual
base dt nodes.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_fixup_phandle(void *fdt, void *fdto, int symbols_off, int property) {`


overlay_fixup_phandle - Set an overlay phandle to the base one
@fdt: Base Device Tree blob
@fdto: Device tree overlay blob
@symbols_off: Node offset of the symbols node in the base device tree
@property: Property offset in the overlay holding the list of fixups

overlay_fixup_phandle() resolves all the overlay phandles pointed
to in a __fixups__ property, and updates them to match the phandles
in use in the base device tree.

This is part of the device tree overlay application process, when
you want all the phandles in the overlay to point to the actual
base dt nodes.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_fixup_phandles(void *fdt, void *fdto) {`


overlay_fixup_phandles - Resolve the overlay phandles to the base
device tree
@fdt: Base Device Tree blob
@fdto: Device tree overlay blob

overlay_fixup_phandles() resolves all the overlay phandles pointing
to nodes in the base device tree.

This is one of the steps of the device tree overlay application
process, when you want all the phandles in the overlay to point to
the actual base dt nodes.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_adjust_local_conflicting_phandle(void *fdto, int node, uint32_t fdt_phandle) {`


overlay_adjust_local_conflicting_phandle: Changes a phandle value
@fdto: Device tree overlay
@node: The node the phandle is set for
@fdt_phandle: The new value for the phandle

returns:
0 on success
Negative error code on failure


---

## `static int overlay_update_node_conflicting_references(void *fdto, int tree_node, int fixup_node, uint32_t fdt_phandle, uint32_t fdto_phandle) {`


overlay_update_node_conflicting_references - Recursively replace phandle values
@fdto: Device tree overlay blob
@tree_node: Node to recurse into
@fixup_node: Node offset of the matching local fixups node
@fdt_phandle: Value to replace phandles with
@fdto_phandle: Value to be replaced

Replaces all phandles with value @fdto_phandle by @fdt_phandle.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_update_local_conflicting_references(void *fdto, uint32_t fdt_phandle, uint32_t fdto_phandle) {`


overlay_update_local_conflicting_references - Recursively replace phandle values
@fdto: Device tree overlay blob
@fdt_phandle: Value to replace phandles with
@fdto_phandle: Value to be replaced

Replaces all phandles with value @fdto_phandle by @fdt_phandle.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_prevent_phandle_overwrite_node(void *fdt, int fdtnode, void *fdto, int fdtonode) {`


overlay_prevent_phandle_overwrite_node - Helper function for overlay_prevent_phandle_overwrite
@fdt: Base Device tree blob
@fdtnode: Node in fdt that is checked for an overwrite
@fdto: Device tree overlay blob
@fdtonode: Node in fdto matching @fdtnode

returns:
0 on success
Negative error code on failure


---

## `static int overlay_prevent_phandle_overwrite(void *fdt, void *fdto) {`


overlay_prevent_phandle_overwrite - Fixes overlay phandles to not overwrite base phandles
@fdt: Base Device Tree blob
@fdto: Device tree overlay blob

Checks recursively if applying fdto overwrites phandle values in the base
dtb. When such a phandle is found, the fdto is changed to use the fdt's
phandle value to not break references in the base.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_apply_node(void *fdt, int target, void *fdto, int node) {`


overlay_apply_node - Merges a node into the base device tree
@fdt: Base Device Tree blob
@target: Node offset in the base device tree to apply the fragment to
@fdto: Device tree overlay blob
@node: Node offset in the overlay holding the changes to merge

overlay_apply_node() merges a node into a target base device tree
node pointed.

This is part of the final step in the device tree overlay
application process, when all the phandles have been adjusted and
resolved and you just have to merge overlay into the base device
tree.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_merge(void *fdt, void *fdto) {`


overlay_merge - Merge an overlay into its base device tree
@fdt: Base Device Tree blob
@fdto: Device tree overlay blob

overlay_merge() merges an overlay into its base device tree.

This is the next to last step in the device tree overlay application
process, when all the phandles have been adjusted and resolved and
you just have to merge overlay into the base device tree.

returns:
0 on success
Negative error code on failure


---

## `static int overlay_symbol_update(void *fdt, void *fdto) {`


overlay_symbol_update - Update the symbols of base tree after a merge
@fdt: Base Device Tree blob
@fdto: Device tree overlay blob

overlay_symbol_update() updates the symbols of the base tree with the
symbols of the applied overlay

This is the last step in the device tree overlay application
process, allowing the reference of overlay symbols by subsequent
overlay operations.

returns:
0 on success
Negative error code on failure


---


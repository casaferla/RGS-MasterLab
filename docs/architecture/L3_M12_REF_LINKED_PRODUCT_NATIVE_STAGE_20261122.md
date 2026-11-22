# L3 M12 first native REF_LINKED Product seam

The existing DSP runtime was MANUAL-only despite documented and
registered REF_LINKED segment availability. REF_LINKED now has
genuine ModuleInstance provenance=REFERENCE_MATCH, owner=
REFERENCE_MATCH, linkState=REF_LINKED and a mandatory nonnil
semanticNodeId, with no change to existing MANUAL/USER/UNLINKED
defaults. A normal manual ProcessingChain cannot contain REF_LINKED;
the REF_LINKED chain cannot contain manual; its normal add/move/
remove/duplicate mutators reject unauthorized structural mutation
while userBypass remains independent.

A real Product registry, ordinary ProcessingChain::restore and
the existing typed ModuleExecutionBinding and actual parameter
factory validate a frozen accepted plan EXACTLY. No inverse
parameter synthesis, no reoptimization, no invented DSP, no gain
(Input Gain), dither or terminal true peak limiter inside the linked
artistic nodes. Original M10 order and final parameter values are
immutable in the stage. Manual DSP chain/bindings are only read.
Source/fixed/safety/anchor/terminal current owner preconditions block
the stage on missing evidence. No-correction with zero artistic
nodes is explicitly allowed only when separately approved.

This is a LIVE DSP native stage, NOT a production persistent
M12 transaction/receipt. Next integrate atomic ProjectDocument
snapshot, controller UUID/plan binding and terminal composition.
A staged node set is NOT MATERIALIZED/NOT_VERIFIED and cannot
claim successful M13 verification until separately committed and
freshly rendered.

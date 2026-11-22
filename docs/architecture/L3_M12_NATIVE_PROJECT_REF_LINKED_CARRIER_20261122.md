# L3 M12 real native ProjectDocument REF_LINKED carrier

Uses the already-present project::Chain and project::Module schema,
ordinary DSP ModuleInstance truth, the single DSP parameter codec
and existing canonical project OpaqueJsonValue without inventing
new per-module ownership fields. Project fields are exactly
instanceId/typeId/enabled/userBypass/controllerSuspended/
domainSuspended/provenance/owner/linkState/semanticNodeId/
algorithmVersion/parameterSchemaId/parameters.

Module type/version/schema come from the actual Product registry,
the semantic node from the stage, and typed parameters are checked
by encoder -> canonical project JSON -> strict decoder round-trip.
Wrong schema, unrecognized DSP, unregistered codec or changed byte
content fails closed. No fallback to a manual factory or truncated
last-known chain. Existing ProjectDocument referenceMatch metadata
must still bind the controller and immutable plan to this ordinary
Chain in a subsequent guarded atomic Product transaction; until
that exists this adapter explicitly does NOT grant project commit,
M12 materialized receipt, execution activation or M13 verified
status.

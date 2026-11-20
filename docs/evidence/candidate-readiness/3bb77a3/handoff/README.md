# Breakwater Windows VM handoff

Application source: `3bb77a353fb04267260764060bc5f58d61d5e7f5`.

This is a review candidate. Owner/license review and actual clean-Windows acceptance are pending.

Read CANDIDATE_SUMMARY.md for this kit's verified identities. The two RELEASE_REVIEW packets describe historical revisions; their results do not qualify this candidate.

1. Verify the ZIP hashes against SHA256SUMS.txt.
2. On a fresh Windows x64 VM, extract the Windows ZIP to a path containing spaces and Unicode.
3. Run Breakwater/Verify-Breakwater.cmd, then Start-Breakwater.cmd. No PowerShell 7, Python or SDK installation is required.
4. Complete CLEAN_WINDOWS_CHECK.md and fill a COPY of clean-windows-result.template.json with actual outcomes.
5. Retain the generated verification folder, screenshots and matching source ZIP.

The automated bundle report deliberately identifies sanitized processes, even when run on a VM. Record the actual VM identity/manual acceptance separately. Neither this kit nor a process check grants release approval.

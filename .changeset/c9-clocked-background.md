---
'lg-luma': patch
---

Use the C9 SMP/ACB hardware video pipeline behind Home so LG controls presentation
timing. Preserve display timestamps, feed ahead, and drain EOS before rotation.
Keep buffered NDL v1 available for rollback and retain the smooth fullscreen app.

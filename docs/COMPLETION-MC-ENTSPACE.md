# GETENTSPACE

MCSERV/XMCSERV 12h fragt nun die tatsächliche Verzeichnisstruktur der Karte ab.
Gezählt werden gelöschte Einträge und bei ungerader Verzeichnislänge der freie
zweite Eintrag des letzten Clusters. Es wird kein FAT-Freiplatz synthetisiert.
Abgleich: PS2SDK `329e2bb43287e16af16c4dde1c614e2662c24468`,
`mcserv.c` Command12h und `main.c`/`mcman_getentspace`.

`test_mc_ent_space` besteht mit geraden/ungeraden Längen, Löschung, regulären
Dateien, fehlendem Namen/Karte, unformatierter Karte, unterminiertem RPC,
Port-/Slotfehlern und schreibgeschütztem Remount. Bestehende FAT-/READ-ABI-
Regressionssuite ebenfalls erfolgreich. Reproduktion über
`tools/build_host_core.py test_mc_ent_space test_ps2_mc_files`.

CheckBlock33h bleibt offen: die Referenz prüft nicht nur Metadaten, sondern
löscht und beschreibt den Block mit Testmustern. Ein nondestruktiver
Attrappen-Erfolg wäre keine korrekte Implementierung. Raw-Page/Erase,
Directory-Descriptors und weitere Ports bleiben eigene Katalogpunkte.

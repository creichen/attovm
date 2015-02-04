/***************************************************************************
  Copyright (C) 2014 Christoph Reichenbach


 This program may be modified and copied freely according to the terms of
 the GNU general public license (GPL), as long as the above copyright
 notice and the licensing information contained herein are preserved.

 Please refer to www.gnu.org for licensing details.

 This work is provided AS IS, without warranty of any kind, expressed or
 implied, including but not limited to the warranties of merchantability,
 noninfringement, and fitness for a specific purpose. The author will not
 be held liable for any damage caused by this work or derivatives of it.

 By using this source code, you agree to the licensing terms as stated
 above.


 Please contact the maintainer for bug reports or inquiries.

 Current Maintainer:

    Christoph Reichenbach (CR) <creichen@gmail.com>

***************************************************************************/

#ifndef _ATTOL_ADDRESS_STORE_H
#define _ATTOL_ADDRESS_STORE_H

/* Adressenspeicher, zur verbesserten Disassemblierung */

#define ADDRSTORE_PUT(entity, kind) addrstore_put(entity, ADDRSTORE_KIND_ ## kind, #entity);

#define ADDRSTORE_KIND_TYPE		0
#define ADDRSTORE_KIND_FUNCTION		1
#define ADDRSTORE_KIND_BUILTIN		2
#define ADDRSTORE_KIND_SPECIAL		3
#define ADDRSTORE_KIND_DATA		4
#define ADDRSTORE_KIND_STRING_LITERAL	5
#define ADDRSTORE_KIND_TRAMPOLINE	6
#define ADDRSTORE_KIND_COUNTER		7

/**
 * Legt einen neuen Eintrag im Adressenspeicher ab
 *
 * @param addr Die zu merkende Adresse
 * @param kind Art der Adresse (ADDRSTORE_KIND_*)
 * @param description Name des Eintrages
 */
void
addrstore_put(void *addr, int kind, char *description);

/**
 * Sucht einen Praefix für einen Adresseintrag
 *
 * @param addr Die zu suchende Adresse
 * @return Einen beschreibenden Praefix (ADDRSTORE_KIND_*), oder NULL
 */
char *
addrstore_get_prefix(void *);

/**
 * Sucht den Namen eines Adresseintrages
 *
 * @param addr Die zu suchende Adresse
 * @return Der zugeordnete Name, oder NULL
 */
char *
addrstore_get_suffix(void *);


#endif // !defined(_ATTOL_ADDRESS_STORE_H)

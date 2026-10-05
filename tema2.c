/*Popa Marian Eduard - 314CC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ID 64
#define MAX_WORD 105
#define MAX_LINE 5000000

#define ALPH 26
#define TERM 26
#define NR_FII 27

/* nod pentru lista de cuvinte-cheie a unui fisier */
typedef struct kw {
	char word[MAX_WORD];
	struct kw *next;
} KwNode;

/* nod pentru lista dublu inlantuita de fisiere */
typedef struct fnode {
	char id[MAX_ID];
	int score;
	KwNode *kw_head;
	struct fnode *prev;
	struct fnode *next;
} FileNode;

/* structura listei dublu inlantuite */
typedef struct {
	FileNode *head;
	FileNode *tail;
} FileList;

/* lista de referinte din nodurile terminale ale arborelui catre fisiere */
typedef struct fref {
	FileNode *file;
	struct fref *next;
} FileRef;

/* nodul din arborele Trie - are 26 de fii pentru litere + slot pentru terminator */
typedef struct tnode {
	struct tnode *fii[NR_FII];
	FileRef *refs;
} TrieNode;

/* structura pentru Max-Heap folosit la TOPK */
typedef struct {
	FileNode **v;
	int n;
	int cap;
} Heap;

/* aloca si initializeaza o lista de fisiere goala */
FileList *fl_create(void)
{
	FileList *l = malloc(sizeof(FileList));
	if (l == NULL) {
		fprintf(stderr, "Eroare la alocarea listei de fisiere\n");
		return NULL;
	}
	l->head = NULL;
	l->tail = NULL;
	return l;
}

/* cauta liniar un fisier dupa ID in lista */
FileNode *fl_find(FileList *l, const char *id)
{
	if (l == NULL)
		return NULL;

	FileNode *p = l->head;
	while (p != NULL) {
		if (strcmp(p->id, id) == 0)
			return p;
		p = p->next;
	}
	return NULL;
}

/* adauga un fisier nou la sfarsitul listei dublu inlantuite */
FileNode *fl_add(FileList *l, const char *id, int score)
{
	if (l == NULL)
		return NULL;

	FileNode *n = malloc(sizeof(FileNode));
	if (n == NULL) {
		fprintf(stderr, "Eroare la alocarea unui nod de fisier\n");
		return NULL;
	}

	strcpy(n->id, id);
	n->score = score;
	n->kw_head = NULL;
	n->next = NULL;
	/* leg noul nod la fostul tail */
	n->prev = l->tail;

	if (l->tail != NULL)
		l->tail->next = n;
	else
		l->head = n;
	l->tail = n;

	return n;
}

/* verifica daca un cuvant exista deja in lista de cuvinte-cheie a unui fisier */
int kw_contains(KwNode *h, const char *word)
{
	KwNode *p = h;
	while (p != NULL) {
		if (strcmp(p->word, word) == 0)
			return 1;
		p = p->next;
	}
	return 0;
}

/* adauga un cuvant la inceputul listei daca nu exista deja */
int kw_add(KwNode **h, const char *word)
{
	if (kw_contains(*h, word))
		return 0;

	KwNode *n = malloc(sizeof(KwNode));
	if (n == NULL) {
		fprintf(stderr, "Eroare la alocarea cuvantului-cheie\n");
		return 0;
	}

	strcpy(n->word, word);
	/* inserare la inceput, e mai rapid decat la final */
	n->next = *h;
	*h = n;
	return 1;
}

/* elimina un cuvant din lista de cuvinte-cheie a unui fisier */
int kw_remove(KwNode **h, const char *word)
{
	KwNode *p = *h;
	KwNode *prev = NULL;

	while (p != NULL) {
		if (strcmp(p->word, word) == 0) {
			/* daca e primul nod, mut head-ul */
			if (prev == NULL)
				*h = p->next;
			else
				prev->next = p->next;
			free(p);
			return 1;
		}
		prev = p;
		p = p->next;
	}
	return 0;
}

/* elibereaza intreaga lista de cuvinte-cheie */
void kw_free_all(KwNode *h)
{
	KwNode *p = h;
	while (p != NULL) {
		KwNode *aux = p->next;
		free(p);
		p = aux;
	}
}

/* scoate un fisier din lista dublu inlantuita si elibereaza memoria */
void fl_remove(FileList *l, FileNode *node)
{
	if (l == NULL || node == NULL)
		return;

	/* refac legaturile vecinilor sa "sara" peste nodul curent */
	if (node->prev != NULL)
		node->prev->next = node->next;
	else
		l->head = node->next;

	if (node->next != NULL)
		node->next->prev = node->prev;
	else
		l->tail = node->prev;

	kw_free_all(node->kw_head);
	free(node);
}

/* elibereaza toata lista de fisiere la finalul programului */
void fl_free(FileList *l)
{
	if (l == NULL)
		return;

	FileNode *p = l->head;
	while (p != NULL) {
		FileNode *aux = p->next;
		kw_free_all(p->kw_head);
		free(p);
		p = aux;
	}
	free(l);
}

/* aloca un nod nou de Trie cu toti fiii pe NULL */
TrieNode *trie_new_node(void)
{
	TrieNode *n = malloc(sizeof(TrieNode));
	if (n == NULL) {
		fprintf(stderr, "Eroare la alocare nod trie\n");
		return NULL;
	}
	for (int i = 0; i < NR_FII; i++)
		n->fii[i] = NULL;
	n->refs = NULL;
	return n;
}

/* elibereaza intreaga lista de referinte dintr-un nod terminal */
void elibereaza_refs(FileRef *r)
{
	while (r != NULL) {
		FileRef *aux = r->next;
		free(r);
		r = aux;
	}
}

/* elibereaza recursiv tot arborele Trie */
void trie_free(TrieNode *root)
{
	if (root == NULL)
		return;
	/* merg pe toti fiii (doar literele, nu si terminatorul) */
	for (int i = 0; i < ALPH; i++)
		if (root->fii[i] != NULL)
			trie_free(root->fii[i]);
	elibereaza_refs(root->refs);
	free(root);
}

/* verifica daca un fisier are deja o referinta in nodul terminal */
int ref_exista(FileRef *h, FileNode *f)
{
	FileRef *p = h;
	while (p != NULL) {
		if (p->file == f)
			return 1;
		p = p->next;
	}
	return 0;
}

/* adauga o referinta noua la inceputul listei daca nu exista deja */
int ref_adauga(FileRef **h, FileNode *f)
{
	if (ref_exista(*h, f))
		return 0;

	FileRef *n = malloc(sizeof(FileRef));
	if (n == NULL) {
		fprintf(stderr, "Eroare la alocare referinta\n");
		return 0;
	}
	n->file = f;
	n->next = *h;
	*h = n;
	return 1;
}

/* sterge o referinta catre un fisier specific din lista */
int ref_sterge(FileRef **h, FileNode *f)
{
	FileRef *p = *h;
	FileRef *prev = NULL;
	while (p != NULL) {
		if (p->file == f) {
			if (prev == NULL)
				*h = p->next;
			else
				prev->next = p->next;
			free(p);
			return 1;
		}
		prev = p;
		p = p->next;
	}
	return 0;
}

/* insereaza un cuvant in Trie si adauga referinta catre fisier in nodul terminal */
int trie_insert(TrieNode *root, const char *word, FileNode *file)
{
	if (root == NULL || word == NULL)
		return 0;

	TrieNode *cur = root;
	/* parcurg fiecare litera si creez nod nou daca nu exista */
	for (int i = 0; word[i] != '\0'; i++) {
		int idx = word[i] - 'a';
		if (idx < 0 || idx >= ALPH)
			return 0;
		if (cur->fii[idx] == NULL) {
			cur->fii[idx] = trie_new_node();
			if (cur->fii[idx] == NULL)
				return 0;
		}
		cur = cur->fii[idx];
	}

	/* marchez nodul ca terminal prin slot-ul TERM */
	cur->fii[TERM] = cur;
	return ref_adauga(&cur->refs, file);
}

/* cauta in Trie un cuvant complet si returneaza nodul terminal sau NULL */
TrieNode *trie_find(TrieNode *root, const char *word)
{
	if (root == NULL || word == NULL)
		return NULL;

	TrieNode *cur = root;
	for (int i = 0; word[i] != '\0'; i++) {
		int idx = word[i] - 'a';
		if (idx < 0 || idx >= ALPH)
			return NULL;
		if (cur->fii[idx] == NULL)
			return NULL;
		cur = cur->fii[idx];
	}
	/* verific ca nodul gasit e chiar marcat ca terminal */
	if (cur->fii[TERM] == NULL)
		return NULL;
	return cur;
}

/* cauta nodul corespunzator unui prefix - nu trebuie sa fie terminal */
TrieNode *trie_find_prefix(TrieNode *root, const char *prefix)
{
	if (root == NULL || prefix == NULL)
		return NULL;

	TrieNode *cur = root;
	for (int i = 0; prefix[i] != '\0'; i++) {
		int idx = prefix[i] - 'a';
		if (idx < 0 || idx >= ALPH)
			return NULL;
		if (cur->fii[idx] == NULL)
			return NULL;
		cur = cur->fii[idx];
	}
	return cur;
}

/* verifica daca un nod nu mai are niciun fiu */
int nu_mai_are_fii(TrieNode *node)
{
	for (int i = 0; i < ALPH; i++)
		if (node->fii[i] != NULL)
			return 0;
	return 1;
}

/* sterge recursiv un cuvant din Trie eliminand nodurile inutile */
int trie_delete_word(TrieNode *node, const char *word, int pos)
{
    if (node == NULL)
        return 0;

    /* am ajuns la finalul cuvantului - demarcam nodul terminal */
    if (word[pos] == '\0') {
        node->fii[TERM] = NULL;
    } else {
        int idx = word[pos] - 'a';
        if (idx < 0 || idx >= ALPH)
            return 0;
        if (node->fii[idx] == NULL)
            return 0;

        /* daca fiul poate fi sters, il sterg si curat pointer-ul */
        if (trie_delete_word(node->fii[idx], word, pos + 1)) {
            free(node->fii[idx]);
            node->fii[idx] = NULL;
        }
    }

    /* nu sterg nodul daca e terminal pentru alt cuvant */
    if (node->fii[TERM] != NULL)
        return 0;

    /* nu sterg nodul daca mai are alti fii */
    for (int i = 0; i < ALPH; i++)
        if (node->fii[i] != NULL)
            return 0;

    return 1;
}

/* elimina referinta unui fisier din Trie si curata nodurile ramase goale */
void trie_remove_ref(TrieNode *root, const char *word, FileNode *file)
{
    TrieNode *t = trie_find(root, word);
    if (t == NULL)
        return;

    ref_sterge(&t->refs, file);

    /* daca nodul terminal a ramas fara referinte, sterg cuvantul din arbore */
    if (t->refs == NULL)
        trie_delete_word(root, word, 0);
}

/* aloca un Heap nou cu o capacitate initiala */
Heap *heap_create(int cap)
{
	if (cap <= 0)
		cap = 8;

	Heap *h = malloc(sizeof(Heap));
	if (h == NULL) {
		fprintf(stderr, "Eroare la alocare heap\n");
		return NULL;
	}

	h->v = malloc(cap * sizeof(FileNode *));
	if (h->v == NULL) {
		fprintf(stderr, "Eroare la alocare vector heap\n");
		free(h);
		return NULL;
	}

	h->n = 0;
	h->cap = cap;
	return h;
}

/* elibereaza memoria heap-ului */
void heap_free(Heap *h)
{
	if (h == NULL)
		return;
	free(h->v);
	free(h);
}

/* verifica daca heap-ul e gol */
int heap_empty(Heap *h)
{
	if (h == NULL)
		return 1;
	return h->n == 0;
}

/* compara doua fisiere pentru ordinea din heap: scor mai mare = prioritate mai mare */
int mai_mare(FileNode *a, FileNode *b)
{
	if (a->score != b->score)
		return a->score > b->score;
	/* la scor egal, ordine lexicografica dupa ID (mai mic = prioritate mai mare) */
	return strcmp(a->id, b->id) < 0;
}

/* schimba doi pointeri intre ei */
void swap_fn(FileNode **x, FileNode **y)
{
	FileNode *aux = *x;
	*x = *y;
	*y = aux;
}

/* urca un element in heap pana ajunge in pozitia corecta*/
void urca(Heap *h, int i)
{
	while (i > 0) {
		int p = (i - 1) / 2;
		if (mai_mare(h->v[i], h->v[p])) {
			swap_fn(&h->v[i], &h->v[p]);
			i = p;
		} else {
			break;
		}
	}
}

/* coboara un element in heap pana ajunge in pozitia corecta */
void coboara(Heap *h, int i)
{
	while (1) {
		int st = 2 * i + 1;
		int dr = 2 * i + 2;
		int best = i;

		/* aleg cel mai mare dintre nod si cei doi fii */
		if (st < h->n && mai_mare(h->v[st], h->v[best]))
			best = st;
		if (dr < h->n && mai_mare(h->v[dr], h->v[best]))
			best = dr;

		if (best == i)
			break;
		swap_fn(&h->v[i], &h->v[best]);
		i = best;
	}
}

/* insereaza un fisier in heap si reorganizeaza */
void heap_insert(Heap *h, FileNode *f)
{
	if (h == NULL || f == NULL)
		return;

	/* daca vectorul e plin, dublez capacitatea */
	if (h->n == h->cap) {
		int new_cap = h->cap * 2;
		FileNode **aux = realloc(h->v, new_cap * sizeof(FileNode *));
		if (aux == NULL) {
			fprintf(stderr, "Eroare la realocare heap\n");
			return;
		}
		h->v = aux;
		h->cap = new_cap;
	}

	h->v[h->n] = f;
	h->n++;
	urca(h, h->n - 1);
}

/* extrage maximul din heap si se reorganizeaza */
FileNode *heap_extract_max(Heap *h)
{
	if (heap_empty(h))
		return NULL;

	FileNode *top = h->v[0];
	h->n--;
	/* mut ultimul element pe pozitia 0 si il cobor */
	if (h->n > 0) {
		h->v[0] = h->v[h->n];
		coboara(h, 0);
	}
	return top;
}

/* comparator pentru qsort - sortare lexicografica dupa ID */
int cmp_lex(const void *a, const void *b) {
    FileNode *fa = *(FileNode **)a;
    FileNode *fb = *(FileNode **)b;
    return strcmp(fa->id, fb->id);
}

void sorteaza_lex(FileNode **v, int n) {
    qsort(v, n, sizeof(FileNode *), cmp_lex);
}

/* numara cate referinte sunt intr-o lista */
int numara_refs(FileRef *r)
{
	int c = 0;
	while (r != NULL) {
		c++;
		r = r->next;
	}
	return c;
}

/* comparator pentru sortarea cuvintelor primite la ADD */
int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

/* operatia ADD - adauga un fisier nou cu cuvintele sale cheie */
void op_add(FileList *fl, TrieNode *t, char *args, FILE *out)
{
    char *tok = strtok(args, " \t\n");
    if (tok == NULL) return;
    char id[MAX_ID];
    strcpy(id, tok);

    tok = strtok(NULL, " \t\n");
    if (tok == NULL) return;
    int score = atoi(tok);

    tok = strtok(NULL, " \t\n");
    if (tok == NULL) return;
    int nrc = atoi(tok);

    /* daca fisierul exista deja, consum cuvintele si afisez EXISTS */
    if (fl_find(fl, id) != NULL) {
        for (int i = 0; i < nrc; i++)
            strtok(NULL, " \t\n");
        fprintf(out, "EXISTS\n");
        return;
    }

    /* salvez cuvintele intr-un vector ca sa le sortez si sa elimin duplicatele */
    char **kws = malloc(nrc * sizeof(char *));
    if (kws == NULL)
        return;

    int count = 0;
    for (int i = 0; i < nrc; i++) {
        tok = strtok(NULL, " \t\n");
        if (tok == NULL) break;
        kws[count++] = tok;
    }

    if (count > 0)
        qsort(kws, count, sizeof(char *), cmp_str);

    FileNode *f = fl_add(fl, id, score);
    if (f == NULL) {
        free(kws);
        return;
    }

    /* dupa sortare, duplicatele sunt consecutive - le sar peste */
    for (int i = 0; i < count; i++) {
        if (i == 0 || strcmp(kws[i], kws[i - 1]) != 0) {
            KwNode *n = malloc(sizeof(KwNode));
            if (n != NULL) {
                strcpy(n->word, kws[i]);
                n->next = f->kw_head;
                f->kw_head = n;
                trie_insert(t, kws[i], f);
            }
        }
    }

    free(kws);
    fprintf(out, "OK\n");
}  

/* operatia DEL - sterge un fisier si toate referintele lui din Trie */
void op_del(FileList *fl, TrieNode *t, char *args, FILE *out)
{
	char *tok = strtok(args, " \t\n");
	if (tok == NULL)
		return;

	FileNode *f = fl_find(fl, tok);
	if (f == NULL) {
		fprintf(out, "NOT FOUND\n");
		return;
	}

	/* parcurg toate cuvintele fisierului si scot referintele din Trie */
	KwNode *p = f->kw_head;
	while (p != NULL) {
		trie_remove_ref(t, p->word, f);
		p = p->next;
	}

	fl_remove(fl, f);
	fprintf(out, "OK\n");
}

/* operatia ADDKW - adauga un cuvant cheie nou unui fisier existent */
void op_addkw(FileList *fl, TrieNode *t, char *args, FILE *out)
{
	char *id_tok = strtok(args, " \t\n");
	char *w_tok = strtok(NULL, " \t\n");
	if (id_tok == NULL || w_tok == NULL)
		return;

	FileNode *f = fl_find(fl, id_tok);
	if (f == NULL) {
		fprintf(out, "NOT FOUND\n");
		return;
	}

	/* daca exista deja, e OK dar nu fac nimic */
	if (kw_contains(f->kw_head, w_tok)) {
		fprintf(out, "OK\n");
		return;
	}

	kw_add(&f->kw_head, w_tok);
	trie_insert(t, w_tok, f);
	fprintf(out, "OK\n");
}

/* operatia DELKW - sterge un cuvant cheie dintr-un fisier */
void op_delkw(FileList *fl, TrieNode *t, char *args, FILE *out)
{
	char *id_tok = strtok(args, " \t\n");
	char *w_tok = strtok(NULL, " \t\n");
	if (id_tok == NULL || w_tok == NULL)
		return;

	FileNode *f = fl_find(fl, id_tok);
	if (f == NULL) {
		fprintf(out, "NOT FOUND\n");
		return;
	}

	/* daca cuvantul nu e in lista, e OK dar nu modific nimic */
	if (!kw_contains(f->kw_head, w_tok)) {
		fprintf(out, "OK\n");
		return;
	}

	trie_remove_ref(t, w_tok, f);
	kw_remove(&f->kw_head, w_tok);
	/* daca fisierul ramane fara niciun cuvant cheie, il sterg si pe el */
	if (f->kw_head == NULL) {
		fl_remove(fl, f);
	}
	fprintf(out, "OK\n");
}

/* operatia FIND - cauta in Trie si afiseaza fisierele asociate unui cuvant */
void op_find(TrieNode *t, char *args, FILE *out)
{
	char *tok = strtok(args, " \t\n");
	if (tok == NULL)
		return;

	TrieNode *term = trie_find(t, tok);
	if (term == NULL || term->refs == NULL) {
		fprintf(out, "EMPTY\n");
		return;
	}

	/* trec referintele intr-un vector ca sa pot sorta lexicografic */
	int n = numara_refs(term->refs);
	FileNode **v = malloc(n * sizeof(FileNode *));
	if (v == NULL) {
		fprintf(stderr, "Eroare la alocare vector FIND\n");
		return;
	}

	int i = 0;
	FileRef *r = term->refs;
	while (r != NULL) {
		v[i++] = r->file;
		r = r->next;
	}

	sorteaza_lex(v, n);

	fprintf(out, "%d ", n);
	for (i = 0; i < n; i++)
		fprintf(out, "%s ", v[i]->id);
	fprintf(out, "\n");

	free(v);
}

/* operatia TOPK - foloseste Max-Heap pentru a obtine primele k fisiere dupa scor */
void op_topk(TrieNode *t, char *args, FILE *out)
{
	char *w_tok = strtok(args, " \t\n");
	char *k_tok = strtok(NULL, " \t\n");
	if (w_tok == NULL || k_tok == NULL)
		return;
	int k = atoi(k_tok);

	TrieNode *term = trie_find(t, w_tok);
	if (term == NULL || term->refs == NULL || k <= 0) {
		fprintf(out, "EMPTY\n");
		return;
	}

	int n = numara_refs(term->refs);
	Heap *h = heap_create(n + 1);
	if (h == NULL)
		return;

	/* bag toate referintele in heap */
	FileRef *r = term->refs;
	while (r != NULL) {
		heap_insert(h, r->file);
		r = r->next;
	}

	/* daca cer mai mult decat exista, afisez doar cate sunt */
	int luate = (k < n) ? k : n;
	FileNode **rez = malloc(luate * sizeof(FileNode *));
	if (rez == NULL) {
		fprintf(stderr, "Eroare la alocare rezultat TOPK\n");
		heap_free(h);
		return;
	}

	/* extrag primele k maxime din heap */
	for (int i = 0; i < luate; i++)
		rez[i] = heap_extract_max(h);

	fprintf(out, "%d ", luate);
	for (int i = 0; i < luate; i++)
		fprintf(out, "%s ", rez[i]->id);
	fprintf(out, "\n");

	free(rez);
	heap_free(h);
}

/* parcurge recursiv arborele si afiseaza fiecare cuvant in ordine lexicografica */
void print_rec(TrieNode *node, char *buf, int adanc, FILE *out, int *gata)
{
	if (node == NULL)
		return;

	/* daca am ajuns la un nod terminal cu referinte, il afisez */
	if (node->fii[TERM] != NULL && node->refs != NULL) {
		buf[adanc] = '\0';
		int n = numara_refs(node->refs);
		FileNode **v = malloc(n * sizeof(FileNode *));
		if (v == NULL) {
			fprintf(stderr, "Eroare la alocare PRINT\n");
			return;
		}

		int j = 0;
		FileRef *r = node->refs;
		while (r != NULL) {
			v[j++] = r->file;
			r = r->next;
		}
		sorteaza_lex(v, n);

		fprintf(out, "%s %d ", buf, n);
		for (j = 0; j < n; j++)
			fprintf(out, "%s ", v[j]->id);
		fprintf(out, "\n");
		free(v);
		*gata = 1;
	}

	/* DFS in ordine alfabetica (de la 'a' la 'z') */
	for (int i = 0; i < ALPH; i++) {
		if (node->fii[i] != NULL) {
			buf[adanc] = 'a' + i;
			print_rec(node->fii[i], buf, adanc + 1, out, gata);
		}
	}
}

/* operatia PRINT - afiseaza toate cuvintele din arbore in ordine lexicografica */
void op_print(TrieNode *t, FILE *out)
{
	char buf[MAX_WORD];
	int gata = 0;

	print_rec(t, buf, 0, out, &gata);
	if (!gata)
		fprintf(out, "EMPTY\n");
}

/* parcurge subarborele si aduna toate fisierele din nodurile terminale intalnite */
void aduna_fisiere(TrieNode *node, FileNode ***v, int *n, int *cap)
{
    if (node == NULL)
        return;

    /* la fiecare nod terminal, adaug toate referintele in vectorul rezultat */
    if (node->fii[TERM] != NULL && node->refs != NULL) {
        FileRef *r = node->refs;
        while (r != NULL) {
            /* dublez capacitatea daca s-a umplut vectorul */
            if (*n == *cap) {
                *cap = (*cap == 0) ? 8 : (*cap) * 2;
                FileNode **aux = realloc(*v, (*cap) * sizeof(FileNode *));
                if (aux == NULL) return;
                *v = aux;
            }
            (*v)[*n] = r->file;
            (*n)++;
            r = r->next;
        }
    }

    for (int i = 0; i < ALPH; i++)
        if (node->fii[i] != NULL)
            aduna_fisiere(node->fii[i], v, n, cap);
}

/* operatia PREFIX-cauta toate fisierele cu cuvinte ce incep cu prefixul dat */
void op_prefix(TrieNode *t, char *args, FILE *out)
{
    char *tok = strtok(args, " \t\n");
    if (tok == NULL) return;

    /* gasesc nodul de unde porneste prefixul; daca nu exista, nu am rezultate */
    TrieNode *node = trie_find_prefix(t, tok);
    if (node == NULL) {
        fprintf(out, "EMPTY\n");
        return;
    }

    FileNode **v = NULL;
    int n = 0, cap = 0;
    aduna_fisiere(node, &v, &n, &cap);

    if (n == 0) {
        fprintf(out, "EMPTY\n");
        free(v);
        return;
    }

    sorteaza_lex(v, n);

    /* elimin duplicatele - un fisier poate aparea prin mai multe cuvinte cu acelasi prefix */
    int uniq = 0;
    for (int i = 0; i < n; i++) {
        if (uniq == 0 || strcmp(v[i]->id, v[uniq - 1]->id) != 0) {
            v[uniq++] = v[i];
        }
    }

    fprintf(out, "%d ", uniq);
    for (int i = 0; i < uniq; i++)
        fprintf(out, "%s ", v[i]->id);
    fprintf(out, "\n");

    free(v);
}

/* functia principala - citeste comenzile si le dispatcheaza la functiile corespunzatoare */
int main(void)
{
	FILE *in = fopen("indexare.in", "r");
	if (in == NULL) {
		fprintf(stderr, "Nu s-a putut deschide indexare.in\n");
		return 1;
	}

	FILE *out = fopen("indexare.out", "w");
	if (out == NULL) {
		fprintf(stderr, "Nu s-a putut deschide indexare.out\n");
		fclose(in);
		return 1;
	}

	/* initializez lista de fisiere si radacina arborelui Trie */
	FileList *fl = fl_create();
	if (fl == NULL) {
		fclose(in);
		fclose(out);
		return 1;
	}

	TrieNode *t = trie_new_node();
	if (t == NULL) {
		fl_free(fl);
		fclose(in);
		fclose(out);
		return 1;
	}

	int q;
	if (fscanf(in, "%d", &q) != 1) {
		fprintf(stderr, "Format invalid pentru numarul de operatii\n");
		trie_free(t);
		fl_free(fl);
		fclose(in);
		fclose(out);
		return 1;
	}
	fgetc(in); /* consum newline-ul ramas dupa numarul de operatii */

	/* aloc dinamic linia ca sa pot citi comenzi ADD cu multe cuvinte */
	char *line = malloc(MAX_LINE * sizeof(char));
	if (line == NULL) {
		fprintf(stderr, "Eroare alocare line\n");
		return 1;
	}

	int citite = 0;
	while (citite < q && fgets(line, MAX_LINE, in) != NULL) {
		/* curat \n si \r de la final */
		int len = strlen(line);
		while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
			line[len - 1] = '\0';
			len--;
		}
		if (len == 0) continue;

		/* primul token e comanda, restul liniei sunt argumentele */
		char *cmd = strtok(line, " \t");
		if (cmd == NULL) continue;

		char *rest = strtok(NULL, "");
		if (rest == NULL) rest = "";

		int executed = 0;

		/* dispatch pentru fiecare tip de comanda */
		if (strcmp(cmd, "ADD") == 0) {
			op_add(fl, t, rest, out);
			executed = 1;
		} else if (strcmp(cmd, "DEL") == 0) {
			op_del(fl, t, rest, out);
			executed = 1;
		} else if (strcmp(cmd, "ADDKW") == 0) {
			op_addkw(fl, t, rest, out);
			executed = 1;
		} else if (strcmp(cmd, "DELKW") == 0) {
			op_delkw(fl, t, rest, out);
			executed = 1;
		} else if (strcmp(cmd, "FIND") == 0) {
			op_find(t, rest, out);
			executed = 1;
		} else if (strcmp(cmd, "TOPK") == 0) {
			op_topk(t, rest, out);
			executed = 1;
		} else if (strcmp(cmd, "PRINT") == 0) {
			op_print(t, out);
			executed = 1;
		} else if (strcmp(cmd, "PREFIX") == 0) {
			op_prefix(t, rest, out);
			executed = 1;
		}

		if (executed) {
			citite++;
		}
	}

	/* eliberez tot - linie, arbore, lista, fisiere */
	free(line);
	trie_free(t);
	fl_free(fl);
	fclose(in);
	fclose(out);
	return 0;
}

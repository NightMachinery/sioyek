/*
 * Stress test for cross-document key comparison in the shared fz_store.
 *
 * Mirrors how sioyek uses mupdf: one base context with real lock callbacks,
 * and one fz_clone_context per worker thread. Every worker repeatedly opens
 * the same PDF as its own document, renders page 0, and drops the document.
 * All clones share one fz_store, so a worker's shading lookups scan keys
 * that belong to the other workers' documents, some of which are being
 * dropped at that moment.
 *
 * Usage: stress FILE.pdf [THREADS] [SECONDS]
 * Exit status is 0 when every worker finished without a mupdf error.
 */

#include <mupdf/fitz.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static pthread_mutex_t mutexes[FZ_LOCK_MAX];

static void lock_cb(void *user, int lock)
{
	(void)user;
	pthread_mutex_lock(&mutexes[lock]);
}

static void unlock_cb(void *user, int lock)
{
	(void)user;
	pthread_mutex_unlock(&mutexes[lock]);
}

struct worker
{
	pthread_t thread;
	int id;
	fz_context *ctx;
	const char *path;
	double deadline;
	long iterations;
	long errors;
};

static double now_seconds(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void *worker_main(void *arg)
{
	struct worker *w = arg;
	fz_context *ctx = w->ctx;

	while (now_seconds() < w->deadline)
	{
		fz_document *doc = NULL;
		fz_pixmap *pix = NULL;

		fz_var(doc);
		fz_var(pix);
		fz_try(ctx)
		{
			doc = fz_open_document(ctx, w->path);
			pix = fz_new_pixmap_from_page_number(ctx, doc, 0,
				fz_scale(0.5f, 0.5f), fz_device_rgb(ctx), 0);
		}
		fz_always(ctx)
		{
			fz_drop_pixmap(ctx, pix);
			fz_drop_document(ctx, doc);
		}
		fz_catch(ctx)
		{
			fprintf(stderr, "worker %d: %s\n", w->id, fz_caught_message(ctx));
			w->errors++;
		}
		w->iterations++;
	}
	return NULL;
}

int main(int argc, char **argv)
{
	const char *path;
	int nthreads = 4;
	double seconds = 20;
	fz_locks_context locks;
	fz_context *base;
	struct worker *workers;
	long total = 0, errors = 0;
	int i;

	if (argc < 2)
	{
		fprintf(stderr, "usage: %s FILE.pdf [THREADS] [SECONDS]\n", argv[0]);
		return 2;
	}
	path = argv[1];
	if (argc > 2)
		nthreads = atoi(argv[2]);
	if (argc > 3)
		seconds = atof(argv[3]);
	if (nthreads < 1)
		nthreads = 1;

	for (i = 0; i < FZ_LOCK_MAX; i++)
		pthread_mutex_init(&mutexes[i], NULL);
	locks.user = NULL;
	locks.lock = lock_cb;
	locks.unlock = unlock_cb;

	base = fz_new_context(NULL, &locks, FZ_STORE_DEFAULT);
	if (!base)
	{
		fprintf(stderr, "cannot create mupdf context\n");
		return 2;
	}
	fz_register_document_handlers(base);
	/* The workers' failures are reported by the exit status; mupdf's own
	 * warnings would only be noise here. */
	fz_set_warning_callback(base, NULL, NULL);

	workers = calloc(nthreads, sizeof *workers);
	for (i = 0; i < nthreads; i++)
	{
		workers[i].id = i;
		workers[i].ctx = fz_clone_context(base);
		workers[i].path = path;
		workers[i].deadline = now_seconds() + seconds;
		if (!workers[i].ctx)
		{
			fprintf(stderr, "cannot clone mupdf context\n");
			return 2;
		}
	}

	printf("stress: %s, %d threads, %.0f s\n", path, nthreads, seconds);
	fflush(stdout);

	for (i = 0; i < nthreads; i++)
		pthread_create(&workers[i].thread, NULL, worker_main, &workers[i]);
	for (i = 0; i < nthreads; i++)
	{
		pthread_join(workers[i].thread, NULL);
		total += workers[i].iterations;
		errors += workers[i].errors;
		fz_drop_context(workers[i].ctx);
	}
	fz_drop_context(base);
	free(workers);

	printf("stress: done, %ld open/render/drop iterations, %ld errors\n", total, errors);
	return errors ? 1 : 0;
}

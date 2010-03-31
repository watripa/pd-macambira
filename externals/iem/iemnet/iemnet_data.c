/* iemnet
 * data handling code
 *  - wrappers for data "chunks"
 *  - queues
 *
 *  copyright (c) 2010 IOhannes m zm�lnig, IEM
 */

//#define DEBUG

#include "iemnet.h"
#include "iemnet_data.h"

#include <string.h>
#include <stdio.h>

#include <sys/types.h>

#include <pthread.h>



#define INBUFSIZE 65536L /* was 4096: size of receiving data buffer */


/* data handling */

t_iemnet_floatlist*iemnet__floatlist_init(t_iemnet_floatlist*cl) {
  unsigned int i;
  if(NULL==cl)return NULL;
  for(i=0; i<cl->size; i++)
    SETFLOAT((cl->argv+i), 0.f);

  return cl;
}

void iemnet__floatlist_destroy(t_iemnet_floatlist*cl) {
  if(NULL==cl)return;
  if(cl->argv) freebytes(cl->argv, sizeof(t_atom)*cl->size);
  cl->argv=NULL;
  cl->argc=0;
  cl->size=0;

  freebytes(cl, sizeof(t_iemnet_floatlist));
}

t_iemnet_floatlist*iemnet__floatlist_create(unsigned int size) {
  t_iemnet_floatlist*result=(t_iemnet_floatlist*)getbytes(sizeof(t_iemnet_floatlist));
  if(NULL==result)return NULL;

  result->argv = (t_atom*)getbytes(size*sizeof(t_atom));
  if(NULL==result->argv) {
    iemnet__floatlist_destroy(result);
    return NULL;
  }

  result->argc=size;
  result->size=size;

  result=iemnet__floatlist_init(result);

  return result;
}

t_iemnet_floatlist*iemnet__floatlist_resize(t_iemnet_floatlist*cl, unsigned int size) {
  t_atom*tmp;
  if(size<=cl->size) {
    cl->argc=size;
    return cl;
  }

  tmp=(t_atom*)getbytes(size*sizeof(t_atom));
  if(NULL==tmp) return NULL;

  freebytes(cl->argv, sizeof(t_atom)*cl->size);

  cl->argv=tmp;
  cl->argc=cl->size=size;

  cl=iemnet__floatlist_init(cl);

  return cl;
}

void iemnet__chunk_destroy(t_iemnet_chunk*c) {
  if(NULL==c)return;

  if(c->data)freebytes(c->data, c->size*sizeof(unsigned char));

  c->data=NULL;
  c->size=0;

  freebytes(c, sizeof(t_iemnet_chunk));
}

t_iemnet_chunk* iemnet__chunk_create_empty(int size) {
  t_iemnet_chunk*result=(t_iemnet_chunk*)getbytes(sizeof(t_iemnet_chunk));
  if(result) {
    result->size=size;
    result->data=(unsigned char*)getbytes(sizeof(unsigned char)*size); 

    if(NULL == result->data) {
      result->size=0;
      iemnet__chunk_destroy(result);
      return NULL;
    }

    memset(result->data, 0, result->size);

    result->addr=0L;
    result->port=0;

  }
  return result;
}

t_iemnet_chunk* iemnet__chunk_create_data(int size, unsigned char*data) {
  t_iemnet_chunk*result=iemnet__chunk_create_empty(size);
  if(result) {
    memcpy(result->data, data, result->size);
  }
  return result;
}

t_iemnet_chunk* iemnet__chunk_create_dataaddr(int size, 
					      unsigned char*data,
					      struct sockaddr_in*addr) {
  t_iemnet_chunk*result=iemnet__chunk_create_data(size, data);
  if(addr) {
    result->addr = ntohl(addr->sin_addr.s_addr);
    result->port = ntohs(addr->sin_port);
  }
  return result;
}

t_iemnet_chunk* iemnet__chunk_create_list(int argc, t_atom*argv) {
  int i;
  t_iemnet_chunk*result=iemnet__chunk_create_empty(argc);
  if(NULL==result)return NULL;

  for(i=0; i<argc; i++) {
    unsigned char c = atom_getint(argv);
    result->data[i]=c;
    argv++;
  }

  return result;
}

t_iemnet_chunk*iemnet__chunk_create_chunk(t_iemnet_chunk*c) {
  t_iemnet_chunk*result=NULL;
  if(NULL==c)return NULL;
  result=iemnet__chunk_create_data(c->size, c->data);
  result->addr=c->addr;
  result->port=c->port;

  return result;
}


t_iemnet_floatlist*iemnet__chunk2list(t_iemnet_chunk*c, t_iemnet_floatlist*dest) {
  unsigned int i;
  if(NULL==c)return NULL;
  dest=iemnet__floatlist_resize(dest, c->size);
  if(NULL==dest)return NULL;

  for(i=0; i<c->size; i++) {
    dest->argv[i].a_w.w_float = c->data[i];
  }

  return dest;
}


/* queue handling */

/*
 *   using code found at http://newsgroups.derkeiler.com/Archive/Comp/comp.programming.threads/2008-02/msg00502.html
 */


#ifdef t_iemnet_queue
# undef t_iemnet_queue
#endif
typedef struct _node {
  struct _node* next;
  t_iemnet_chunk*data;
} t_node;

struct _iemnet_queue {
  t_node* head; /* = 0 */
  t_node* tail; /* = 0 */
  pthread_mutex_t mtx;
  pthread_cond_t cond;

  int done; // in cleanup state
  int size;
};


int queue_push(
                      t_iemnet_queue* const _this,
                      t_iemnet_chunk* const data
                      ) {
  t_node* tail;
  t_node* n=NULL;
  int size=_this->size;

  if(NULL == data) return size;
  //fprintf(stderr, "pushing %d bytes\n", data->size);

  n=(t_node*)getbytes(sizeof(t_node));

  n->next = 0;
  n->data = data;
  pthread_mutex_lock(&_this->mtx);
  if (! (tail = _this->tail)) {
    _this->head = n;
  } else {
    tail->next = n;
  }
  _this->tail = n;

  _this->size+=data->size;
  size=_this->size;


  //fprintf(stderr, "pushed %d bytes\n", data->size);

  pthread_mutex_unlock(&_this->mtx);
  pthread_cond_signal(&_this->cond);

  return size;
}

t_iemnet_chunk* queue_pop_block(
                   t_iemnet_queue* const _this
                   ) {
  t_node* head=0;
  t_iemnet_chunk*data=0;
  pthread_mutex_lock(&_this->mtx);
  while (! (head = _this->head)) {
    if(_this->done) {
      pthread_mutex_unlock(&_this->mtx);
      return NULL;
    }
    else {
      pthread_cond_wait(&_this->cond, &_this->mtx);
    }
  }

  if (! (_this->head = head->next)) {
    _this->tail = 0;
  }
  if(head && head->data) {
    _this->size-=head->data->size;
  }

  pthread_mutex_unlock(&_this->mtx);
  if(head) {
    data=head->data;
    freebytes(head, sizeof(t_node));
    head=NULL;
  }
  return data;
}

t_iemnet_chunk* queue_pop_noblock(
                   t_iemnet_queue* const _this
                   ) {
  t_node* head=0;
  t_iemnet_chunk*data=0;
  pthread_mutex_lock(&_this->mtx);
  if (! (head = _this->head)) {
    // empty head
    pthread_mutex_unlock(&_this->mtx);
    return NULL;
  }
  if (! (_this->head = head->next)) {
    _this->tail = 0;
  }
  if(head && head->data) {
    _this->size-=head->data->size;
  }

  pthread_mutex_unlock(&_this->mtx);
  if(head) {
    data=head->data;
    freebytes(head, sizeof(t_node));
    head=NULL;
  }
  return data;
}

t_iemnet_chunk* queue_pop(t_iemnet_queue* const _this) {
  return queue_pop_block(_this);
}

int queue_getsize(t_iemnet_queue* const _this) {
  int size=-1;
  if(_this) {
    pthread_mutex_lock(&_this->mtx);
    size=_this->size;
    pthread_mutex_unlock(&_this->mtx);
  }
  return size;
}
void queue_finish(t_iemnet_queue* q) {
  DEBUG("queue_finish: %x", q);
  if(NULL==q) 
    return;
  q->done=1;
  DEBUG("queue signaling: %x", q);
  pthread_cond_signal(&q->cond);
  DEBUG("queue signaled", q);
}

void queue_destroy(t_iemnet_queue* q) {
  t_iemnet_chunk*c=NULL;
  if(NULL==q) 
    return;

  queue_finish(q);

  /* remove all the chunks from the queue */
  while(NULL!=(c=queue_pop_noblock(q))) {
    iemnet__chunk_destroy(c);
  }

  q->head=NULL;
  q->tail=NULL;

  pthread_mutex_destroy(&q->mtx);
  pthread_cond_destroy(&q->cond);

  freebytes(q, sizeof(t_iemnet_queue));
  q=NULL;
}

t_iemnet_queue* queue_create(void) {
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

  t_iemnet_queue*q=(t_iemnet_queue*)getbytes(sizeof(t_iemnet_queue));
  if(NULL==q)return NULL;

  q->head = NULL;
  q->tail = NULL;

  memcpy(&q->cond, &cond, sizeof(pthread_cond_t));
  memcpy(&q->mtx , &mtx, sizeof(pthread_mutex_t));

  q->done = 0;
  q->size = 0;

  return q;
}
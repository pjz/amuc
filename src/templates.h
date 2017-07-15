void alert(const char *form,...);
extern bool debug;

template <class T,uint dim>
struct Array {
  T buf[dim];
  uint len() { return dim; }
  uint get_index(T& memb) {
    for (uint ind=0;ind<dim;++ind)
      if (buf[ind]==memb) return ind;
    return -1;
  }
  T& operator[](uint ind) {
    if (ind<dim) return buf[ind];
    alert("Array: index=%d (>=%d)",ind,dim);// if (debug) abort();
    return buf[0];
  }
  void operator=(T *val) { for (uint i=0;i<dim;++i) buf[i]=val[i]; }
};

template<class T>
struct SLList_elem {    // single-linked list element
  T d;
  SLList_elem<T>* nxt;
  SLList_elem(T& d1):d(d1),nxt(0) { }
  ~SLList_elem() { delete nxt; }
};

template<class T>     // single-linked list
struct SLinkedList {
  SLList_elem<T> *lis;
  SLinkedList() { lis=0; }
  ~SLinkedList() { delete lis; }
  void reset() { delete lis; lis=0; }
  SLList_elem<T> *insert(T elm, bool incr) {  // if incr then increasing
    SLList_elem<T> *p,*p1,
                   *ret=0;
    if (!lis)
      lis=ret=new SLList_elem<T>(elm);
    else if (elm==lis->d);
    else if ((incr && (elm<lis->d)) || (!incr && (lis->d<elm))) {
      p1=ret=new SLList_elem<T>(elm);
      p1->nxt=lis; lis=p1;
    }
    else {
      for (p=lis;;p=p->nxt) {
        if (p->d==elm) break;
        if (!p->nxt) {
          p->nxt=ret=new SLList_elem<T>(elm);
          break;
        }
        if ((incr && (elm < p->nxt->d)) || (!incr && (p->nxt->d < elm))) {
          p1=ret=new SLList_elem<T>(elm);
          p1->nxt=p->nxt; p->nxt=p1;
          break;
        }
      }
    }
    return ret;
  }
  SLList_elem<T> *prepend(T elm) {
    SLList_elem<T> *p,
                   *ret;
    if (!lis)
      lis=ret=new SLList_elem<T>(elm);
    else {
      p=ret=new SLList_elem<T>(elm);
      p->nxt=lis; lis=p;
    }
    return ret;
  }
  void remove(T elm) {
    SLList_elem<T> *p,*prev;
    if (!lis) return;
    if (lis->d==elm) {
      p=lis->nxt; lis->nxt=0; delete lis; lis=p;
      return;
    }
    for (prev=lis,p=lis->nxt;p;) {
      if (p->d==elm) {
        prev->nxt=p->nxt; p->nxt=0; delete p;
        return;
      }
      else { prev=p; p=p->nxt; }
    }
    puts("SLL: remove: elm not found");
  }
  SLList_elem<T> *remove(SLList_elem<T> *p) { // returns next element
    SLList_elem<T> *p1,*prev;
    if (!lis) { puts("SLL: lis=0"); return 0; }
    if (lis==p) {
      p1=lis->nxt; lis->nxt=0; delete lis; lis=p1;
      return lis;
    }
    for (prev=lis,p1=lis->nxt;p1;) {
      if (p==p1) {
        prev->nxt=p1->nxt; p1->nxt=0; delete p1;
        return prev->nxt;
      }
      prev=p1; p1=p1->nxt;
    }
    puts("SLL: remove: ptr not found");
    return 0;
  }
  void invert() {
    SLList_elem<T> *p,*prev,*next;
    if (!lis || !lis->nxt) return;
    for (prev=lis,p=lis->nxt,next=p->nxt,lis->nxt=0;;) {
      p->nxt=prev;
      prev=p;
      if (!next) { lis=p; break; }
      p=next;
      next=p->nxt;
    }
  }
};

template<class T>
struct SafeBuffer {
  bool alloced; // buf alloced?
  int size,
      bpos;  // current buf index
  T *buf;
  SafeBuffer():alloced(false),size(0),bpos(0),buf(0) { }
  SafeBuffer(SafeBuffer& src):alloced(false),size(src.size),bpos(0),buf(src.buf) { }
  void operator=(SafeBuffer& src) { buf=src.buf; size=src.size; alloced=false; bpos=0; }
  ~SafeBuffer() { if (alloced) delete[] buf; }
  void reset() {
    if (alloced) delete[] buf;
    buf=0; size=0; bpos=0; alloced=false;
  }
};

template<class T> T* re_alloc(T* arr,int& len) {
  int i;
  T* new_arr=new T[len*2];
  for (i=0;i<len;++i) new_arr[i]=arr[i];
  delete[] arr;
  len*=2;
  return new_arr;
}


#if !(defined _GCOBJECT_H_)
#define _GCOBJECT_H_
#include "nanassert.h"

class GCObject {
private:
    size_t refCount;
protected:
    GCObject() : refCount(0) {
    }
    virtual ~GCObject() {
    }
public:
    void addRef() {
        refCount++;
    }
    void delRef() {
        I(refCount);
        refCount--;
        if(!refCount)
            delete this;
    }
    size_t getRefCount() const {
        return refCount;
    }
};

template<class T>
class SmartPtr {
private:
    T *ref;
public:
    SmartPtr() : ref(0) {
    }
    SmartPtr(const SmartPtr &src) : ref(src.ref) {
        if(ref)
            ref->addRef();
    }
    SmartPtr(T *ptr) : ref(ptr) {
        if(ref)
            ref->addRef();
    }
    ~SmartPtr() {
        if(ref)
            ref->delRef();
    }
    operator T *() const {
        return ref;
    }
    T &operator*() const {
        return *ref;
    }
    T *operator->() const {
        return ref;
    }
    T *operator=(T *ptr) {
        if(ptr)
            ptr->addRef();
        if(ref)
            ref->delRef();
        ref=ptr;
        return ptr;
    }
    SmartPtr<T> &operator=(const SmartPtr<T> &src) {
        (*this)=src.ref;
        return *this;
    }
};
#endif // !(defined _GCOBJECT_H_)

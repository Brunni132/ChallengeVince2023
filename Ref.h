#pragma once

#ifdef __ANDROID__
	#include <cstddef>
	#include <string.h>
	#include <math.h>
#endif
#include <stdio.h>
#include <cstddef>

#ifdef REF_DEBUG
extern unsigned g_allocCount;
#define REF_LOGALLOC(obj)	g_allocCount++;
#define REF_LOGRELEASE(obj)	g_allocCount--;
extern void REF_LOGALLOCT(const char *type);
extern void REF_LOGRELEASET(const char *type);
extern void REF_PRINTREMAINING();
#else
#define REF_LOGALLOC(obj)
#define REF_LOGRELEASE(obj)
#define REF_LOGALLOCT(t)
#define REF_LOGRELEASET(t)
#endif

struct RefClass {
	unsigned __ref_count;		// -1 => freed (an unmanaged object will be left at 0)
	RefClass() : __ref_count(0) { REF_LOGALLOC(this); }
	~RefClass() { REF_LOGRELEASE(this); if ((int)__ref_count > 0) fprintf(stderr, "!!!!!!! Freeing an object that is still retained elsewhere\n"); }
	// Not allowed if not redefined properly!!! (i.e. do not copy the __ref_count)
	RefClass(const RefClass& other);
	RefClass& operator =(const RefClass&other);
};

template<class T>
struct ref {
	T *ptr;

	ref() : ptr(0) {}
	explicit ref(T* eptr) : ptr(eptr) { incref(); }
	ref(std::nullptr_t p) : ptr(0) {}
	ref(T* eptr, bool takeOwnership) : ptr(eptr) { REF_LOGALLOCT(typeid(T).name()); }
	ref(const ref &ref) : ptr(ref.ptr) { incref(); }
	ref(const ref &&other) : ptr(other.ptr) { other.ptr = NULL; }
	ref(ref &&temp) : ptr(temp.ptr) { temp.ptr = nullptr; }
	template<class U>
	ref(const ref<U> &ref) : ptr(ref.ptr) { incref(); }
	/************************************************************************/
	/* If you get an error around here, it means that you are trying to use */
	/* a class that is not inheriting RefClass!                             */
	/************************************************************************/
	~ref() { decref(); }

	// Checked assignment of a standard pointer
	ref& operator <<= (T *eptr) {
		// Not using this logic may free the original object if counter = 1
		incref(eptr);
		decref();
		ptr = eptr;
		return *this;
	}
	ref& operator = (std::nullptr_t p) { decref(); ptr = nullptr; return *this; }
	// Various operator overloading
	template<class U>
	ref<T> &operator = (const ref<U> &ref) { return (*this) <<= ref.ptr; }
	ref& operator = (const ref &ref) { return (*this) <<= ref.ptr; }
	ref& operator = (ref &&temp) { decref(); ptr = temp.ptr; temp.ptr = nullptr; return *this; }
	T* operator -> () const { return ptr; }
	T& operator * () const { return *ptr; }
	operator T * () const { return ptr; }
	bool operator == (const ref &ref) const { return ptr == ref.ptr; }
	bool operator == (T *eptr) const { return ptr == eptr; }

	/**
	 * Call this to get rid of any refs that may delete this object.
	 * Basically this will return a raw pointer to this object and you
	 * will have to delete it manually when you do not need it anymore.
	 * Note that this will render any ref to this object as weak
	 * (potentially dangling once you delete it).
	 */
	T* unmanaged_ptr() { incref(); return ptr; }

private:
	void incref() { if (ptr) ptr->__ref_count++; }
	void incref(T *eptr) { if (eptr) eptr->__ref_count++; }
	void decref() { if (ptr && !ptr->__ref_count--) { REF_LOGRELEASET(typeid(T).name()); delete ptr; } }
};

/**
 * Call this to create a ref that will be the owner of the object. Other refs can be
 * made out of this object, but once no more ref reference this object, it will be
 * deleted. If you keep pointers to this object elsewhere in your code (not through
 * refs), they will become dangling.
 * Normal use is when creating a new object (use newref which does exactly the same)
 * or when managing an object created as a pointer.
 * A *method() {
 *     return new A;
 * }
 * ref<A> myA = owning_ref(method());
 */
template<class T>
ref<T> owning_ref(T *obj) {
	return ref<T>(obj, true);
}

template<class T>
ref<T> mkref(T *detachedRef) {
	return ref<T>(detachedRef);
}

struct __MkrefHelper {
	template<class T>
	ref<T> operator * (T *detachedRef) {
		return ref<T>(detachedRef, true);
	}
};
#define newref __MkrefHelper() * new 
#define thisref mkref(this)
#define nullref NULL

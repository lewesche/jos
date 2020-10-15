// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.

	//see memlayout.h
	unsigned pn = (unsigned) addr/PGSIZE;
	pte_t pte = uvpt[pn];

	if( !(err&FEC_WR) || !(pte&PTE_COW)) // unsure about second part of this check, but idk how to get pte here
		panic("Panic from lib/fork.c pgfault() : check failed");

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.

	//panic("pgfault not implemented");
	//
	r = sys_page_alloc(sys_getenvid(), PFTEMP, PTE_U|PTE_P|PTE_W);
	if(r<0) {panic("Panic from lib/fork.c pgfault() : alloc failed");}

	memcpy(addr, PFTEMP, PGSIZE);

	r = sys_page_map(sys_getenvid(), PFTEMP, sys_getenvid(), addr, PTE_U|PTE_P|PTE_W);
	if(r<0) {panic("Panic from lib/fork.c pgfault() : map failed");}

	r = sys_page_unmap(sys_getenvid(), PFTEMP);
	if(r<0) {panic("Panic from lib/fork.c pgfault() : unmap failed");}

	return;
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	//panic("duppage not implemented");
	
	void *addr = (void*)(pn*PGSIZE);	
	int perm = PTE_U|PTE_P;
	
	//see memlayout.h
	pte_t pte = uvpt[pn];
	if((pte & PTE_W) || (pte & PTE_COW)) {
		perm |= PTE_COW;
	}

	// parent -> child
	r = sys_page_map(sys_getenvid(), addr, envid, addr, perm);
	if(r<0) {panic("Panic from lib/fork.c dupage() : map failed");}

	// parent -> parent
	r = sys_page_map(sys_getenvid(), addr, sys_getenvid(), addr, perm);
	if(r<0) {panic("Panic from lib/fork.c dupage() : map failed");}

	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	// panic("fork not implemented");

	int err;

	set_pgfault_handler(pgfault);
	envid_t child_envid	= sys_exofork(); 

	if(child_envid<0) {panic("Panic from lib/fork.c fork() : exofork failed");}

	if(child_envid ==0) {
		// This is the child
		thisenv = &envs[ENVX(sys_getenvid())]; // from lib/libmain.c
	} else {
		// The parent has the valid address space, so duppage calls go here

		// Fresh page for child UXSTACK
		err = sys_page_alloc(child_envid, (void*)(UXSTACKTOP - PGSIZE), PTE_U | PTE_W | PTE_P);
		if(err<0) {panic("Panic from lib/fork.c fork() : alloc failed");}

		// Everything below UXSTACK -
		for(unsigned pn=0; pn < (UXSTACKTOP-PGSIZE)/PGSIZE; pn++) {
			pte_t pte = uvpt[pn];
			if((pte & PTE_P) && (pte & PTE_U)) {
				// I wrote dupage so that it can handle genuine RO pages
				duppage(child_envid, pn);
			}
			// Not expecing pages arent PTE_U
		}	

		// entry point refers to assembly portion i think
		err = sys_env_set_pgfault_upcall(child_envid, thisenv->env_pgfault_upcall);
		if(err<0) {panic("Panic from lib/fork.c fork() : set_upcall failed");}

		err = sys_env_set_status(child_envid, ENV_RUNNABLE);
		if(err<0) {panic("Panic from lib/fork.c fork() : set_status failed");}
	}

	return child_envid;
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}

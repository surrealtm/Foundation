#include "memutils.h"
#include "os_specific.h"

void Memory_Arena::create(u64 reserved, u64 requested_commit_size) {
	assert(this->base == null);
	assert(reserved != 0);
	
	if(this->base = os_reserve_memory(reserved)) {
		this->page_size   = os_get_page_size();
		this->commit_size = requested_commit_size ? align_to(requested_commit_size, this->page_size, u64) : this->page_size * 3;
		this->reserved    = reserved;
		this->committed   = 0;
		this->size        = 0;
	} else {
		this->page_size   = 0;
		this->commit_size = 0;
		this->reserved    = 0;
		this->committed   = 0;
		this->size        = 0;
	}
}

void Memory_Arena::destroy() {
	assert(this->base != null);
	assert(this->reserved != 0);
	os_free_memory(this->base, this->reserved);
	this->base = null;
	this->size = 0;
	this->reserved = 0;
	this->committed = 0;
	this->page_size = 0;
	this->commit_size = 0;
}

void *Memory_Arena::push(u64 size) {
	if(this->size + size > this->committed) {
		if(this->committed + this->commit_size <= this->reserved) {
			if(os_commit_memory((char *) this->base + this->committed, this->commit_size)) {
				this->committed += this->commit_size;
			} else {
				this->destroy();
				return null;
			}
		} else {
			report_error("The Memory_Arena ran out of reserved space (" PRIu64 "b reserved, " PRIu64 "b committed).", this->reserved, this->committed);
			return null;
		}
	}

	char *pointer = (char *) this->base + this->size;
	this->size += size;
	return pointer;
}
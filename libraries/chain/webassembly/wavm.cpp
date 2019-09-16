#include <eosio/chain/webassembly/wavm.hpp>
#include <eosio/chain/wasm_eosio_constraints.hpp>
#include <eosio/chain/wasm_eosio_injection.hpp>
#include <eosio/chain/apply_context.hpp>
#include <eosio/chain/exceptions.hpp>

#include <vector>
#include <iterator>

struct counter {
   unsigned long long count;
   ~counter() {
      printf("total wasm exectuion time %llu\n", count);
   }
};
static counter the_counter;

namespace eosio { namespace chain { namespace webassembly { namespace wavm {

class wavm_instantiated_module : public wasm_instantiated_module_interface {
   public:
      wavm_instantiated_module(std::vector<uint8_t>&& initial_mem, std::vector<uint8_t>&& wasm, 
                               const digest_type& code_hash, const uint8_t& vm_version, wavm_runtime& wr) :
         _initial_memory(std::move(initial_mem)),
         _wasm(std::move(wasm)),
         _code_hash(code_hash),
         _vm_version(vm_version),
         _wavm_runtime(wr)
      {

      }

      ~wavm_instantiated_module() {
         _wavm_runtime.cc.free_code(_code_hash, _vm_version);
      }

      void apply(apply_context& context) override {
         const code_descriptor* const cd = _wavm_runtime.cc.get_descriptor_for_code(_code_hash, _vm_version, _wasm, _initial_memory);

         unsigned long long start = __builtin_readcyclecounter();
         auto count_it = fc::make_scoped_exit([&start](){
            the_counter.count += __builtin_readcyclecounter() - start;
         });

         _wavm_runtime.exec.execute(*cd, _wavm_runtime.mem, context);
      }

      const std::vector<uint8_t>     _initial_memory;
      const std::vector<uint8_t>     _wasm;
      const digest_type              _code_hash;
      const uint8_t                  _vm_version;
      wavm_runtime&            _wavm_runtime;
};

wavm_runtime::wavm_runtime(const boost::filesystem::path data_dir, const rodeos::config& rodeos_config) : cc(data_dir, rodeos_config), exec(cc) {
}

wavm_runtime::~wavm_runtime() {
}

std::unique_ptr<wasm_instantiated_module_interface> wavm_runtime::instantiate_module(std::vector<uint8_t>&& wasm, std::vector<uint8_t>&& initial_memory,
                                                                                     const digest_type& code_hash, const uint8_t& vm_type, const uint8_t& vm_version) {

   return std::make_unique<wavm_instantiated_module>(std::move(initial_memory), std::move(wasm), code_hash, vm_type, *this);
}

void wavm_runtime::immediately_exit_currently_running_module() {
   RODEOS_MEMORY_PTR_cb_ptr;
   siglongjmp(*cb_ptr->jmp, 1); ///XXX 1 means clean exit
   __builtin_unreachable();
}

}}}}

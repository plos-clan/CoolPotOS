fn main() {
    println!("cargo:rustc-link-lib=static=p");
    println!("cargo:rustc-link-search=native=libs");
}

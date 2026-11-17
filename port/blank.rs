//! The project's blank leaf: what stands in a leaf's place when the judge tries the project's own build and driver
//! (its control). It builds, and panics if it is ever called. No candidate writes this file.

pub fn answer(input: &[u8], out: &mut [u8]) -> Result<usize, ()> {
    let _ = (input, out);
    panic!("the blank leaf answers nothing")
}

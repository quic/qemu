/*
 * QEMU L2VIC Interrupt Controller
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define L2VIC_INT_ENABLEn       0x100   /* Read/Write */
#define L2VIC_INT_ENABLE_CLEARn 0x180   /* Write */
#define L2VIC_INT_ENABLE_SETn   0x200   /* Write */
#define L2VIC_INT_TYPEn         0x280   /* Read/Write */
#define L2VIC_INT_STATUSn       0x380   /* Read */
#define L2VIC_INT_CLEARn        0x400   /* Write */
#define L2VIC_SOFT_INTn         0x480   /* Write */
#define L2VIC_INT_PENDINGn      0x500   /* Read */

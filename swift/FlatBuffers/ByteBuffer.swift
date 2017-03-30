/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// There are 2 #defines that have an impact on performance of this ByteBuffer implementation
//
//      UNSAFE_BYTEBUFFER
//          This will use unsafe code to manipulate the underlying byte array. This
//          can yield a reasonable performance increase.
//
//      BYTEBUFFER_NO_BOUNDS_CHECK
//          This will disable the bounds check asserts to the byte array. This can
//          yield a small performance gain in normal code..
//
// Using UNSAFE_BYTEBUFFER and BYTEBUFFER_NO_BOUNDS_CHECK together can yield a
// performance gain of ~15% for some operations, however doing so is potentially
// dangerous. Do so at your own risk!
//

/// <summary>
/// Class to mimic Java's ByteBuffer which is used heavily in Flatbuffers.
/// </summary>
public final class ByteBuffer {
    private let _buffer : UnsafeMutableRawBufferPointer
    private var _pos : Int;  // Must track start of the buffer.
    
    public var length : Int { get { return _buffer.count } }
    
    public var data : UnsafeMutableRawBufferPointer { get { return _buffer; } }

    public convenience init(capacity: Int) {
        let buffer = UnsafeMutableRawBufferPointer.allocate(count: capacity)
        self.init(buffer: buffer, pos: 0)
    }
    
    private convenience init(buffer: UnsafeMutableRawBufferPointer) {
        self.init(buffer: buffer, pos: 0)
    }
    
    private init(buffer: UnsafeMutableRawBufferPointer, pos: Int) {
        _buffer = buffer
        _pos = pos
    }
    
    deinit {
        _buffer.deallocate()
    }
    
    public var position : Int {
        get { return _pos; }
        set { _pos = newValue; }
    }
    
    public func reset() {
        _pos = 0
    }
    
    @inline(__always)
    private func assertOffsetAndLength(offset: Int, length: Int) {
        assert(offset >= 0 && offset <= _buffer.count - length)
    }
    
    public func putInt8(offset: Int, value: Int8) {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<Int8>.size)
        _buffer[offset] = UInt8(bitPattern: value)
    }
    
    public func putUInt8(offset: Int, value: UInt8) {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt8>.size)
        _buffer[offset] = value
    }
    
    public func putUInt8(offset: Int, value: UInt8, count: Int) {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt8>.size * count)
        for i in 0..<count {
            _buffer[offset + i] = value
        }
    }
    
    // this method exists in order to conform with Java ByteBuffer standards
    public func put(offset: Int, value: UInt8) {
        self.putUInt8(offset: offset, value: value)
    }
    
    // Unsafe but more efficient versions of Put*.
    public func putInt16(offset: Int, value: Int16) {
        putUInt16(offset: offset, value: UInt16(bitPattern: value))
    }
    
    public func putUInt16(offset: Int, value: UInt16) {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt16>.size)
        
        (_buffer.baseAddress! + offset).assumingMemoryBound(to: UInt16.self).pointee = value.littleEndian
    }
    
    public func putInt32(offset: Int, value: Int32) {
        self.putUInt32(offset: offset, value: UInt32(bitPattern: value))
    }
    
    public func putUInt32(offset: Int, value: UInt32) {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt32>.size)
        (_buffer.baseAddress! + offset).assumingMemoryBound(to: UInt32.self).pointee = value.littleEndian
    }
    
    public func putInt64(offset: Int, value: Int64) {
        self.putUInt64(offset: offset, value: UInt64(bitPattern: value))
    }
    
    public func putUInt64(offset: Int, value: UInt64) {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt64>.size)
        
        (_buffer.baseAddress! + offset).assumingMemoryBound(to: UInt64.self).pointee = value.littleEndian
    }
    
    public func putFloat(offset: Int, value: Float) {
        self.putUInt32(offset: offset, value: value.bitPattern)
    }
    
    public func putDouble(offset: Int, value: Double) {
        self.putUInt64(offset: offset, value: value.bitPattern)
    }
    
    
    public func getInt8(at index: Int) -> Int8 {
        assertOffsetAndLength(offset: index, length: MemoryLayout<Int8>.size)
        return Int8(bitPattern: _buffer[index])
    }
    
    public func get(at index: Int) -> UInt8 {
        assertOffsetAndLength(offset: index, length: MemoryLayout<UInt8>.size)
        return _buffer[index]
    }
    
    public func getInt16(at offset: Int) -> Int16 {
        return Int16(bitPattern: self.getUInt16(at: offset))
    }
    
    public func getUInt16(at offset: Int) -> UInt16 {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt16>.size)
        
        return (_buffer.baseAddress! + offset).assumingMemoryBound(to: UInt16.self).pointee.littleEndian
    }
    
    
    public func getInt32(at offset: Int) -> Int32 {
        return Int32(bitPattern: self.getUInt32(at: offset))
    }
    
    public func getUInt32(at offset: Int) -> UInt32 {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt32>.size)
        
        return (_buffer.baseAddress! + offset).assumingMemoryBound(to: UInt32.self).pointee.littleEndian
    }
    
    public func getInt64(at offset: Int) -> Int64 {
        return Int64(bitPattern: self.getUInt64(at: offset))
    }
    
    public func getUInt64(at offset: Int) -> UInt64 {
        assertOffsetAndLength(offset: offset, length: MemoryLayout<UInt64>.size)
        
        return (_buffer.baseAddress! + offset).assumingMemoryBound(to: UInt64.self).pointee.littleEndian
    }
    
    public func getFloat(at offset: Int) -> Float {
        return Float(bitPattern: self.getUInt32(at: offset))
    }
    
    public func getDouble(at offset: Int) -> Double {
        return Double(bitPattern: self.getUInt64(at: offset))
    }
}

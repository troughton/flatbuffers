//
//  Table.swift
//  FlatBuffers
//
//  Created by Thomas Roughton on 30/03/17.
//  Copyright © 2017 Thomas Roughton. All rights reserved.
//

import Foundation

/// <summary>
/// All tables in the generated code derive from this struct, and add their own accessors.
/// </summary>
public struct Table {
    public let bb_pos : Int
    public let bb : ByteBuffer;
    
    public var byteBuffer : ByteBuffer { get { return bb } }
    
    // Look up a field in the vtable, return an offset into the object, or 0 if the field is not
    // present.
    public func __offset(_ vtableOffset: Int) -> Int {
        let vtable = bb_pos - Int(bb.getInt32(at: bb_pos))
        return vtableOffset < Int(bb.getInt16(at: vtable)) ? Int(bb.getInt16(at: vtable + vtableOffset)) : 0;
    }
    
    public static func __offset(_ vtableOffset: Int, _ offset: Int, _ bb: ByteBuffer) -> Int {
        let vtable = bb.length - offset;
        return Int(
            bb.getInt16(at: vtable + vtableOffset -
                Int(
                    bb.getInt32(at: vtable)
                )
        )) + vtable
    }
    
    // Retrieve the relative offset stored at "offset"
    public func __indirect(_ offset: Int) -> Int {
        return offset + Int(bb.getInt32(at: offset))
    }
    
    public static func __indirect(_ offset: Int, _ bb: ByteBuffer) -> Int {
        return offset + Int(bb.getInt32(at: offset));
    }
    
    // Create a String from UTF-8 data stored inside the flatbuffer.
    public func __string(offset: Int) -> String {
        var offset = offset
        offset += Int(bb.getInt32(at: offset))
        let len = Int(bb.getInt32(at: offset))
        let startPos = offset + MemoryLayout<Int32>.size;
        
        var cString = [Int8](repeating: 0, count: len + 1)
        cString.withUnsafeMutableBytes {
            $0.copyBytes(from: UnsafeMutableRawBufferPointer(start: bb.data.baseAddress!.advanced(by: startPos), count: len))
        }
        return String(cString: cString, encoding: .utf8)!
    }
    
    // Get the length of a vector whose offset is stored at "offset" in this object.
    public func __vector_len(_ offset: Int) -> Int {
        var offset = offset
        offset += bb_pos;
        offset += Int(bb.getInt32(at: offset))
        return Int(bb.getInt32(at: offset))
    }
    
    // Get the start of data of a vector whose offset is stored at "offset" in this object.
    public func __vector(_ offset: Int) -> Int {
        var offset = offset
        offset += bb_pos;
        return offset + Int(bb.getInt32(at: offset)) + MemoryLayout<Int32>.size  // data starts after the length
    }
    
    // Get the data of a vector whoses offset is stored at "offset" in this object as an
    // ArraySegment&lt;byte&gt;. If the vector is not present in the ByteBuffer,
    // then a null value will be returned.
    public func __vector_as_arraysegment(_ offset: Int) -> UnsafeMutableRawBufferPointer? {
        let o = self.__offset(offset);
    
        if (0 == o) {
            return nil;
        }
    
        let pos = self.__vector(o);
        let len = self.__vector_len(o);
        return UnsafeMutableRawBufferPointer(start: self.bb.data.baseAddress?.advanced(by: pos), count: len)
    }
    
    // Initialize any Table-derived type to point to the union at the given offset.
    public func __union<T>(_ offset: Int) -> T where T : IFlatbufferObject {
        var offset = offset
        offset += bb_pos;
        let t = T()
        t.__init(offset + Int(bb.getInt32(at: offset)), bb)
        return t;
    }
    
    public static func __has_identifier(_ bb: ByteBuffer, _ ident: String) -> Bool {
        
        assert(ident.utf8.count == FlatBufferConstants.FileIdentifierLength, "FlatBuffers: file identifier must be length \(FlatBufferConstants.FileIdentifierLength)")
    
        for i in 0..<FlatBufferConstants.FileIdentifierLength {
            if ident.utf8[ident.utf8.index(ident.utf8.startIndex, offsetBy: i)] != bb.get(at: bb.position + MemoryLayout<Int32>.size + i) {
                return false
            }
        }
        
        return true
    }
    
    // Compare strings in the ByteBuffer.
    public static func compareStrings(_ offset_1: Int, _ offset_2: Int, _ bb: ByteBuffer) -> Int {
        var offset_1 = offset_1
        var offset_2 = offset_2
        offset_1 += Int(bb.getInt32(at: offset_1))
        offset_2 += Int(bb.getInt32(at: offset_2))
        let len_1 = Int(bb.getInt32(at: offset_1))
        let len_2 = Int(bb.getInt32(at: offset_2))
        let startPos_1 = offset_1 + MemoryLayout<Int32>.size;
        let startPos_2 = offset_2 + MemoryLayout<Int32>.size
        let len = min(len_1, len_2);
        let bbArray = bb.data;
        
        for i in 0..<len {
            let val1 = bbArray[i + startPos_1]
            let val2 = bbArray[i + startPos_2]
            if (val1 != val2) {
                return Int(val1) - Int(val2)
            }
        }
        return len_1 - len_2;
    }
    
    // Compare string from the ByteBuffer with the string object
    public static func compareStrings(_ offset_1: Int, _ key: UnsafeBufferPointer<UInt8>, _ bb: ByteBuffer) -> Int {
            var offset_1 = offset_1
        
        offset_1 += Int(bb.getInt32(at: offset_1))
        let len_1 = Int(bb.getInt32(at: offset_1))
        let len_2 = key.count;
        let startPos_1 = offset_1 + MemoryLayout<Int32>.size;
        let len = min(len_1, len_2);
        let bbArray = bb.data
        
        for i in 0..<len {
            if (bbArray[i + startPos_1] != key[i]) {
                return Int(bbArray[i + startPos_1]) - Int(key[i]);
            }
        }
        
        return len_1 - len_2;
    }
}

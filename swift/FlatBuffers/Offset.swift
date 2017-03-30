//
//  Offset.swift
//  FlatBuffers
//
//  Created by Thomas Roughton on 30/03/17.
//  Copyright © 2017 Thomas Roughton. All rights reserved.
//

public struct Offset<T> {
    public let value : Int
    
    init(_ value: Int) {
        self.value = value
    }
}

public struct VectorOffset {
    public let value : Int
    
    init(_ value: Int) {
        self.value = value
    }
}

public struct StringOffset {
    public let value : Int
    
    init(_ value: Int) {
        self.value = value
    }
}
